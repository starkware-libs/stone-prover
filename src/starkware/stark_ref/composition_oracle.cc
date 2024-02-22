// Copyright 2023 StarkWare Industries Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License").
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.starkware.co/open-source-license/
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.

#include "starkware/stark/composition_oracle.h"

#include <memory>

#include "starkware/channel/annotation_scope.h"
#include "starkware/utils/profiling.h"

namespace starkware {

namespace {

/*
  Given a column index into the concatenation of the traces, finds the trace index and the column
  index within that trace that the original column corresponds to. See SplitMask() for an example.
*/
std::pair<size_t, size_t> ColumnToTraceColumn(size_t column, const std::vector<uint64_t>& widths) {
  for (size_t trace_i = 0; trace_i < widths.size(); ++trace_i) {
    if (column < widths[trace_i]) {
      return {trace_i, column};
    }
    column -= widths[trace_i];
  }
  ASSERT_RELEASE(false, "Column index out of range.");
}

/*
  Given a mask over the concatenation of traces, splits to n_traces different masks for each trace,
  where n_traces is widths.size(). For example, if we have two traces of widths 2 and 4, and the
  mask is:
    {(10,0), (20,4), (30,1), (40,3), (50,3), (60,5)}
  The split masks will be:
    {(10,0), (30,1)}
    {(20,2), (40,1), (50,1), (60,3)}.
*/
std::vector<std::vector<std::pair<int64_t, uint64_t>>> SplitMask(
    gsl::span<const std::pair<int64_t, uint64_t>> mask, const std::vector<uint64_t>& widths) {
  std::vector<std::vector<std::pair<int64_t, uint64_t>>> masks(widths.size());

  for (const auto& [row, col] : mask) {
    const auto& [trace_i, new_col] = ColumnToTraceColumn(col, widths);
    masks[trace_i].emplace_back(row, new_col);
  }

  return masks;
}

/*
  Computes the vector of widths of the traces.
*/
template <typename TracePtrClass>
std::vector<uint64_t> GetWidths(const std::vector<TracePtrClass>& trace_ptrs) {
  std::vector<uint64_t> sizes;
  sizes.reserve(trace_ptrs.size());
  for (const auto& trace_ptr : trace_ptrs) {
    sizes.push_back(trace_ptr->NumColumns());
  }

  return sizes;
}

/*
  Given queries in evaluation domain, and a mask to a specific trace (set of columns), finds the
  needed queries to the trace that cover the mask at all original queries.
*/
std::vector<std::tuple<uint64_t, uint64_t, size_t>> QueriesToTraceQueries(
    gsl::span<const std::pair<uint64_t, uint64_t>> queries,
    gsl::span<const std::pair<int64_t, uint64_t>> trace_mask, const size_t trace_length) {
  const size_t n_trace_bits = SafeLog2(trace_length);

  std::vector<std::tuple<uint64_t, uint64_t, size_t>> trace_queries;
  trace_queries.reserve(trace_mask.size() * queries.size());
  for (const auto& [coset_index, offset] : queries) {
    for (const auto& [mask_row, mask_col] : trace_mask) {
      // Add mask item to query.
      const uint64_t new_offset = BitReverse(
          (BitReverse(offset, n_trace_bits) + mask_row) & (trace_length - 1), n_trace_bits);
      trace_queries.emplace_back(coset_index, new_offset, mask_col);
    }
  }

  return trace_queries;
}

}  // namespace

CompositionOracleProver::CompositionOracleProver(
    MaybeOwnedPtr<const ListOfCosets> evaluation_domain,
    std::vector<MaybeOwnedPtr<CommittedTraceProverBase>> traces,
    gsl::span<const std::pair<int64_t, uint64_t>> mask, MaybeOwnedPtr<const Air> air,
    MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial, ProverChannel* channel)
    : traces_(std::move(traces)),
      mask_(mask.begin(), mask.end()),
      split_masks_(SplitMask(mask, GetWidths(traces_))),
      evaluation_domain_(std::move(evaluation_domain)),
      air_(std::move(air)),
      composition_polynomial_(std::move(composition_polynomial)),
      channel_(channel) {}

FieldElementVector CompositionOracleProver::EvalComposition(uint64_t task_size) const {
  const Field field = evaluation_domain_->CosetsOffsets()[0].GetField();
  const uint64_t trace_length = evaluation_domain_->Group().Size();
  const size_t n_segments = ConstraintsDegreeBound();
  ASSERT_RELEASE(
      n_segments <= evaluation_domain_->NumCosets(),
      "Composition polynomial degree bound is larger than evaluation domain");
  auto evaluation =
      FieldElementVector::MakeUninitialized(field, composition_polynomial_->GetDegreeBound());

  std::vector<std::unique_ptr<std::vector<FieldElementVector>>> storages;
  size_t n_cached_columns = 0;
  storages.reserve(traces_.size());
  for (const auto& trace : traces_) {
    storages.emplace_back(trace->GetLde()->AllocateStorage());
    if (trace->GetLde()->IsCached()) {
      n_cached_columns += trace->NumColumns();
    }
  }

  // Allocate storage for bit reversal.
  const size_t n_total_columns = Sum(GetWidths(traces_));

  std::vector<FieldElementVector> bitrev_storages;
  bitrev_storages.reserve(n_cached_columns);
  for (size_t i = 0; i < n_cached_columns; ++i) {
    bitrev_storages.emplace_back(FieldElementVector::MakeUninitialized(field, trace_length));
  }

  const size_t log_n_cosets = SafeLog2(evaluation_domain_->NumCosets());
  for (uint64_t coset_index = 0; coset_index < n_segments; coset_index++) {
    size_t bitrev_storage_index = 0;
    std::vector<ConstFieldElementSpan> all_evals;
    all_evals.reserve(n_total_columns);

    // Evaluate all traces at the coset.
    for (size_t trace_idx = 0; trace_idx < traces_.size(); ++trace_idx) {
      ProfilingBlock profiling_lde_block("LDE2");
      const std::vector<FieldElementVector>* coset_columns_eval =
          traces_[trace_idx]->GetLde()->EvalOnCoset(coset_index, storages[trace_idx].get());
      profiling_lde_block.CloseBlock();

      ProfilingBlock profiling_block("BitReversal of columns");
      for (size_t col_idx = 0; col_idx < coset_columns_eval->size(); ++col_idx) {
        if (traces_[trace_idx]->GetLde()->IsCached()) {
          ASSERT_RELEASE(
              bitrev_storage_index < bitrev_storages.size(), "Not enough bitrev storages");
          BitReverseVector(coset_columns_eval->at(col_idx), bitrev_storages[bitrev_storage_index]);
          all_evals.emplace_back(bitrev_storages[bitrev_storage_index++]);
        } else {
          BitReverseInPlace(storages[trace_idx]->at(col_idx));
          all_evals.emplace_back(storages[trace_idx]->at(col_idx));
        }
      }
    }

    const size_t coset_natural_index = BitReverse(coset_index, log_n_cosets);
    const FieldElement coset_offset = evaluation_domain_->CosetsOffsets()[coset_natural_index];
    ProfilingBlock composition_block("Actual point-wise computation");
    composition_polynomial_->EvalOnCosetBitReversedOutput(
        coset_offset, all_evals,
        evaluation.AsSpan().SubSpan(coset_index * trace_length, trace_length), task_size);
  }
  return evaluation;
}

void CompositionOracleProver::DecommitQueries(
    const std::vector<std::pair<uint64_t, uint64_t>>& queries) const {
  for (size_t trace_i = 0; trace_i < traces_.size(); ++trace_i) {
    AnnotationScope scope(channel_, "Trace " + std::to_string(trace_i));
    const auto trace_queries =
        QueriesToTraceQueries(queries, split_masks_[trace_i], evaluation_domain_->Group().Size());

    traces_[trace_i]->DecommitQueries(trace_queries);
  }
}

void CompositionOracleProver::EvalMaskAtPoint(
    const FieldElement& point, const FieldElementSpan& output) const {
  ASSERT_RELEASE(output.Size() == mask_.size(), "Wrong output size");
  const Field field = evaluation_domain_->CosetsOffsets()[0].GetField();
  std::vector<FieldElementVector> trace_mask_evaluations;
  trace_mask_evaluations.reserve(traces_.size());
  for (size_t trace_i = 0; trace_i < traces_.size(); ++trace_i) {
    FieldElementVector eval =
        FieldElementVector::MakeUninitialized(field, split_masks_[trace_i].size());
    traces_[trace_i]->EvalMaskAtPoint(split_masks_[trace_i], point, eval);
    trace_mask_evaluations.push_back(std::move(eval));
  }

  const auto& sizes = GetWidths(traces_);
  std::vector<size_t> mask_offset_in_trace(traces_.size());
  for (size_t mask_i = 0; mask_i < mask_.size(); ++mask_i) {
    const auto& mask_col = mask_[mask_i].second;
    const auto& trace_i = ColumnToTraceColumn(mask_col, sizes).first;
    output.Set(mask_i, trace_mask_evaluations[trace_i][mask_offset_in_trace[trace_i]++]);
  }
}

uint64_t CompositionOracleProver::ConstraintsDegreeBound() const {
  const uint64_t trace_length = evaluation_domain_->Group().Size();
  return SafeDiv(composition_polynomial_->GetDegreeBound(), trace_length);
}

size_t CompositionOracleProver::Width() const { return Sum(GetWidths(traces_)); }

std::vector<const CommittedTraceProverBase*> CompositionOracleProver::Traces() const {
  std::vector<const CommittedTraceProverBase*> result;
  result.reserve(traces_.size());
  for (const auto& trace : traces_) {
    result.push_back(trace.get());
  }
  return result;
}

CompositionOracleVerifier::CompositionOracleVerifier(
    MaybeOwnedPtr<const ListOfCosets> evaluation_domain,
    std::vector<MaybeOwnedPtr<const CommittedTraceVerifierBase>> traces,
    gsl::span<const std::pair<int64_t, uint64_t>> mask, MaybeOwnedPtr<const Air> air,
    MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial, VerifierChannel* channel)
    : traces_(std::move(traces)),
      mask_(mask.begin(), mask.end()),
      split_masks_(SplitMask(mask_, GetWidths(traces_))),
      evaluation_domain_(std::move(evaluation_domain)),
      air_(std::move(air)),
      composition_polynomial_(std::move(composition_polynomial)),
      channel_(channel) {}

FieldElementVector CompositionOracleVerifier::VerifyDecommitment(
    std::vector<std::pair<uint64_t, uint64_t>> queries) const {
  std::vector<FieldElementVector> trace_mask_values;
  trace_mask_values.reserve(traces_.size());
  for (size_t trace_i = 0; trace_i < traces_.size(); ++trace_i) {
    AnnotationScope scope(channel_, "Trace " + std::to_string(trace_i));
    const auto trace_queries =
        QueriesToTraceQueries(queries, split_masks_[trace_i], evaluation_domain_->Group().Size());

    trace_mask_values.emplace_back(traces_[trace_i]->VerifyDecommitment(trace_queries));
  }

  // Compute composition polynomial at queries.
  const auto sizes = GetWidths(traces_);
  const size_t log_n_cosets = SafeLog2(evaluation_domain_->NumCosets());
  FieldElementVector oracle_evaluations = FieldElementVector::Make(evaluation_domain_->GetField());
  oracle_evaluations.Reserve(queries.size());
  FieldElementVector neighbors =
      FieldElementVector::MakeUninitialized(evaluation_domain_->GetField(), mask_.size());
  std::vector<size_t> mask_offset_in_trace(traces_.size());

  for (const auto& [coset_index, offset] : queries) {
    // Fetch neighbors from decommitments.
    for (size_t mask_i = 0; mask_i < mask_.size(); ++mask_i) {
      const auto& mask_col = mask_[mask_i].second;
      const auto& trace_i = ColumnToTraceColumn(mask_col, sizes).first;
      neighbors.Set(mask_i, trace_mask_values[trace_i][mask_offset_in_trace[trace_i]++]);
    }

    // Evaluate composition polynomial at point, given neighbors.
    const size_t coset_natural_index = BitReverse(coset_index, log_n_cosets);
    const FieldElement point = evaluation_domain_->ElementByIndex(coset_natural_index, offset);
    oracle_evaluations.PushBack(composition_polynomial_->EvalAtPoint(point, neighbors));
  }

  return oracle_evaluations;
}

uint64_t CompositionOracleVerifier::ConstraintsDegreeBound() const {
  const uint64_t trace_length = evaluation_domain_->Group().Size();
  return SafeDiv(composition_polynomial_->GetDegreeBound(), trace_length);
}

size_t CompositionOracleVerifier::Width() const { return Sum(GetWidths(traces_)); }

}  // namespace starkware
