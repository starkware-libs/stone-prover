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

#include "starkware/stark/committed_trace.h"

#include <map>

#include "starkware/algebra/fields/field_operations_helper.h"
#include "starkware/utils/profiling.h"

namespace starkware {

namespace {

std::unique_ptr<CachedLdeManager> CreateLdeManager(
    const CachedLdeManager::Config& config, const FftBases& trace_domain,
    const ListOfCosets& evaluation_domain) {
  std::unique_ptr<LdeManager> lde_manager = MakeLdeManager(trace_domain);

  const Field& field = evaluation_domain.GetField();
  FieldElementVector coset_offsets =
      FieldElementVector::MakeUninitialized(field, evaluation_domain.NumCosets());

  size_t log_cosets = SafeLog2(evaluation_domain.NumCosets());
  for (uint64_t i = 0; i < coset_offsets.Size(); ++i) {
    coset_offsets.Set(i, evaluation_domain.CosetsOffsets()[BitReverse(i, log_cosets)]);
  }

  // Create CachedLdeManager.
  return std::make_unique<CachedLdeManager>(
      config, TakeOwnershipFrom(std::move(lde_manager)), UseMovedValue(std::move(coset_offsets)));
}

}  // namespace

CommittedTraceProver::CommittedTraceProver(
    const CachedLdeManager::Config& cached_lde_config,
    MaybeOwnedPtr<const ListOfCosets> evaluation_domain, size_t n_columns,
    const TableProverFactory& table_prover_factory)
    : cached_lde_config_(cached_lde_config),
      evaluation_domain_(std::move(evaluation_domain)),
      n_columns_(n_columns),
      table_prover_(table_prover_factory(
          evaluation_domain_->NumCosets(), evaluation_domain_->Group().Size(), n_columns_)) {}

void CommittedTraceProver::Commit(Trace&& trace, const FftBases& trace_domain, bool bit_reverse) {
  // LDE.
  const Field field = evaluation_domain_->GetField();

  ASSERT_RELEASE(trace.Width() == n_columns_, "Wrong number of columns");

  lde_ = CreateLdeManager(cached_lde_config_, trace_domain, *evaluation_domain_);
  {
    ProfilingBlock interpolation_block("Interpolation");
    auto columns = std::move(trace).ConsumeAsColumnsVector();
    for (auto&& column : columns) {
      if (bit_reverse) {
        BitReverseInPlace(column);
      }

      lde_->AddEvaluation(std::move(column));
    }
  }

  lde_->FinalizeAdding();

  auto storage = lde_->AllocateStorage();
  for (uint64_t coset_index = 0; coset_index < evaluation_domain_->NumCosets(); coset_index++) {
    ProfilingBlock lde_block("LDE");
    // Compute the LDE.
    auto lde_evaluations = lde_->EvalOnCoset(coset_index, storage.get());
    lde_block.CloseBlock();
    // Commit to the LDE.
    ProfilingBlock commit_to_lde_block("Commit to LDE");
    table_prover_->AddSegmentForCommitment(
        {lde_evaluations->begin(), lde_evaluations->end()}, coset_index);
    commit_to_lde_block.CloseBlock();
  }

  table_prover_->Commit();
}

/*
  The queries are tuples of (coset_index, offset, column_index).
*/
void CommittedTraceProver::DecommitQueries(
    gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const {
  const Field field = evaluation_domain_->GetField();
  const uint64_t trace_length = evaluation_domain_->Group().Size();

  // The commitment items we need to open.
  std::set<RowCol> data_queries;
  for (const auto& [coset_index, offset, column_index] : queries) {
    ASSERT_RELEASE(coset_index < evaluation_domain_->NumCosets(), "Coset index out of range");
    ASSERT_RELEASE(offset < trace_length, "Coset offset out of range");
    ASSERT_RELEASE(column_index < NumColumns(), "Column index out of range");
    data_queries.emplace(coset_index * trace_length + offset, column_index);
  }

  // Commitment rows to fetch.
  const std::vector<uint64_t> rows_to_fetch =
      table_prover_->StartDecommitmentPhase(data_queries, {});

  // Prepare storage for the requested rows.
  std::vector<FieldElementVector> elements_data;
  elements_data.reserve(NumColumns());
  for (size_t i = 0; i < NumColumns(); i++) {
    elements_data.push_back(FieldElementVector::MakeUninitialized(field, rows_to_fetch.size()));
  }

  AnswerQueries(rows_to_fetch, &elements_data);

  table_prover_->Decommit(
      std::vector<ConstFieldElementSpan>{elements_data.begin(), elements_data.end()});
}

void CommittedTraceProver::EvalMaskAtPoint(
    gsl::span<const std::pair<int64_t, uint64_t>> mask, const FieldElement& point,
    const FieldElementSpan& output) const {
  const Field field = evaluation_domain_->GetField();
  const FieldElement& trace_gen = evaluation_domain_->TraceGenerator();

  ASSERT_RELEASE(mask.size() == output.Size(), "Wrong output size");

  // A map from column index to pairs (mask_row_offset, mask_index).
  std::map<uint64_t, std::vector<std::pair<int64_t, size_t>>> columns;
  for (size_t mask_index = 0; mask_index < mask.size(); ++mask_index) {
    const auto& [row_offset, column_index] = mask[mask_index];
    ASSERT_RELEASE(row_offset >= 0, "EvalMaskAtPoint() does not support negative mask rows");
    columns[column_index].emplace_back(row_offset, mask_index);
  }

  for (const auto& [column_index, offsets] : columns) {
    // Compute points to evaluate at.
    FieldElementVector points = FieldElementVector::Make(field);
    points.Reserve(offsets.size());
    for (const auto& offset_pair : offsets) {
      const int64_t row_offset = offset_pair.first;
      points.PushBack(point * trace_gen.Pow(row_offset));
    }

    // Allocate output.
    FieldElementVector column_output = FieldElementVector::MakeUninitialized(field, offsets.size());

    // Evaluate.
    lde_->EvalAtPointsNotCached(column_index, points, column_output);

    // Place outputs at correct place.
    for (size_t i = 0; i < offsets.size(); ++i) {
      const size_t mask_index = offsets[i].second;
      output.Set(mask_index, column_output.At(i));
    }
  }
}

void CommittedTraceProver::AnswerQueries(
    const std::vector<uint64_t>& rows_to_fetch, std::vector<FieldElementVector>* output) const {
  const uint64_t trace_length = evaluation_domain_->Group().Size();
  std::vector<std::pair<uint64_t, uint64_t>> coset_and_point_indices;
  const Field field = evaluation_domain_->GetField();

  // Translate queries to coset and point indices.
  coset_and_point_indices.reserve(rows_to_fetch.size());
  for (const uint64_t row : rows_to_fetch) {
    const uint64_t segment = row / trace_length;
    const uint64_t offset = row % trace_length;

    coset_and_point_indices.emplace_back(segment, offset);
  }

  // Call CachedLdeManager::EvalAtPoints().
  auto output_spans = std::vector<FieldElementSpan>{output->begin(), output->end()};
  lde_->EvalAtPoints(coset_and_point_indices, output_spans);
}

CommittedTraceVerifier::CommittedTraceVerifier(
    MaybeOwnedPtr<const ListOfCosets> evaluation_domain, size_t n_columns,
    const TableVerifierFactory& table_verifier_factory, bool should_verify_base_field)
    : evaluation_domain_(std::move(evaluation_domain)),
      n_columns_(n_columns),
      table_verifier_(table_verifier_factory(
          evaluation_domain_->GetField(), evaluation_domain_->Size(), n_columns)),
      should_verify_base_field_(should_verify_base_field) {
  ASSERT_RELEASE(
      !should_verify_base_field_ || IsExtensionField(evaluation_domain_->GetField()),
      "use_extension_field is true but the field is not in the form of "
      "ExtensionFieldElement<>.");
}

void CommittedTraceVerifier::ReadCommitment() { table_verifier_->ReadCommitment(); }

FieldElementVector CommittedTraceVerifier::VerifyDecommitment(
    gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const {
  const uint64_t trace_length = evaluation_domain_->Group().Size();
  // The commitment items we need to open.
  std::set<RowCol> data_queries;
  for (const auto& [coset_index, offset, column_index] : queries) {
    ASSERT_RELEASE(coset_index < evaluation_domain_->NumCosets(), "Coset index out of range");
    ASSERT_RELEASE(offset < trace_length, "Coset offset out of range");
    ASSERT_RELEASE(column_index < n_columns_, "Column index out of range");
    data_queries.emplace(coset_index * trace_length + offset, column_index);
  }

  std::map<RowCol, FieldElement> data_responses =
      table_verifier_->Query(data_queries, {} /* no integrity queries */);

  const Field field = evaluation_domain_->GetField();
  // Check that the trace elements are elements of the base field, in case of usage of extension
  // field. Otherwise, this is always true.
  if (should_verify_base_field_) {
    InvokeFieldTemplateVersion(
        [&](auto field_tag) {
          using FieldElementT = typename decltype(field_tag)::type;
          for (const auto& [key, elm] : data_responses) {
            (void)key;
            ASSERT_RELEASE(
                elm.template As<FieldElementT>().InBaseField(),
                "There is an element in the trace which is not in the base field.");
          }
        },
        field);
  }
  ASSERT_RELEASE(
      table_verifier_->VerifyDecommitment(data_responses),
      "Prover responses did not pass integrity check: Proof rejected.");

  FieldElementVector query_responses = FieldElementVector::Make(field);
  query_responses.Reserve(queries.size());

  for (const auto& [coset_index, offset, column_index] : queries) {
    query_responses.PushBack(
        data_responses.at(RowCol(coset_index * trace_length + offset, column_index)));
  }

  return query_responses;
}

}  // namespace starkware
