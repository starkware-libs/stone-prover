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

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/fraction_field_element.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/bit_reversal.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

template <typename AirT>
void CompositionPolynomialImpl<AirT>::Builder::AddPeriodicColumn(
    PeriodicColumn<FieldElementT> column, size_t periodic_column_index) {
  ASSERT_RELEASE(
      !periodic_columns_[periodic_column_index].has_value(), "Cannot set periodic column twice");
  periodic_columns_[periodic_column_index].emplace(std::move(column));
}

template <typename AirT>
CompositionPolynomialImpl<AirT> CompositionPolynomialImpl<AirT>::Builder::Build(
    MaybeOwnedPtr<const AirT> air, const FieldElementT& trace_generator, const uint64_t coset_size,
    gsl::span<const FieldElementT> random_coefficients, gsl::span<const uint64_t> point_exponents,
    gsl::span<const FieldElementT> shifts) {
  std::vector<PeriodicColumn<FieldElementT>> periodic_columns;
  periodic_columns.reserve(periodic_columns_.size());
  for (size_t i = 0; i < periodic_columns_.size(); ++i) {
    ASSERT_RELEASE(
        periodic_columns_[i].has_value(),
        "Uninitialized periodic column at index " + std::to_string(i));
    periodic_columns.push_back(std::move(periodic_columns_[i].value()));
  }
  return CompositionPolynomialImpl(
      std::move(air), trace_generator, coset_size, periodic_columns, random_coefficients,
      point_exponents, shifts);
}

template <typename AirT>
std::unique_ptr<CompositionPolynomialImpl<AirT>>
CompositionPolynomialImpl<AirT>::Builder::BuildUniquePtr(
    MaybeOwnedPtr<const AirT> air, const FieldElementT& trace_generator, const uint64_t coset_size,
    gsl::span<const FieldElementT> random_coefficients, gsl::span<const uint64_t> point_exponents,
    gsl::span<const FieldElementT> shifts) {
  return std::make_unique<CompositionPolynomialImpl<AirT>>(Build(
      std::move(air), trace_generator, coset_size, random_coefficients, point_exponents, shifts));
}

template <typename AirT>
CompositionPolynomialImpl<AirT>::CompositionPolynomialImpl(
    MaybeOwnedPtr<const AirT> air, FieldElementT trace_generator, uint64_t coset_size,
    std::vector<PeriodicColumn<FieldElementT>> periodic_columns,
    gsl::span<const FieldElementT> coefficients, gsl::span<const uint64_t> point_exponents,
    gsl::span<const FieldElementT> shifts)
    : air_(std::move(air)),
      trace_generator_(std::move(trace_generator)),
      coset_size_(coset_size),
      periodic_columns_(std::move(periodic_columns)),
      coefficients_(coefficients.begin(), coefficients.end()),
      point_exponents_(point_exponents.begin(), point_exponents.end()),
      shifts_(shifts.begin(), shifts.end()) {
  ASSERT_RELEASE(
      coefficients.size() == air_->NumRandomCoefficients(), "Wrong number of coefficients.");

  ASSERT_RELEASE(
      IsPowerOfTwo(coset_size_), "Only cosets of size which is a power of two are supported.");
  const uint64_t generator_order = coset_size;
  ASSERT_RELEASE(
      Pow(trace_generator_, generator_order) == FieldElementT::One(),
      "Provided generator does not generate a group of expected size.");
}

template <typename AirT>
FieldElement CompositionPolynomialImpl<AirT>::EvalAtPoint(
    const FieldElement& point, const ConstFieldElementSpan& neighbors) const {
  return FieldElement(EvalAtPoint(point.As<FieldElementT>(), neighbors.As<FieldElementT>()));
}

template <typename AirT>
auto CompositionPolynomialImpl<AirT>::EvalAtPoint(
    const FieldElementT& point, gsl::span<const FieldElementT> neighbors) const -> FieldElementT {
  std::vector<FieldElementT> periodic_column_vals;
  periodic_column_vals.reserve(periodic_columns_.size());
  for (const PeriodicColumn<FieldElementT>& column : periodic_columns_) {
    periodic_column_vals.push_back(column.EvalAtPoint(point));
  }

  std::vector<FieldElementT> point_powers =
      FieldElementT::UninitializedVector(1 + point_exponents_.size());
  point_powers[0] = point;
  BatchPow(point, point_exponents_, gsl::make_span(point_powers).subspan(1));

  return air_
      ->ConstraintsEval(
          neighbors, periodic_column_vals, coefficients_, point, shifts_,
          air_->DomainEvalsAtPoint(point_powers, shifts_))
      .ToBaseFieldElement();
}

template <typename AirT>
void CompositionPolynomialImpl<AirT>::EvalOnCosetBitReversedOutput(
    const FieldElement& coset_offset, gsl::span<const ConstFieldElementSpan> trace_lde,
    const FieldElementSpan& out_evaluation, uint64_t task_size) const {
  std::vector<gsl::span<const FieldElementT>> trace_spans;
  trace_spans.reserve(trace_lde.size());
  for (const ConstFieldElementSpan& span : trace_lde) {
    trace_spans.push_back(span.As<FieldElementT>());
  }

  MultiplicativeNeighbors<FieldElementT> neighbors(air_->GetMask(), trace_spans);
  EvalOnCosetBitReversedOutput(
      coset_offset.As<FieldElementT>(), neighbors, out_evaluation.As<FieldElementT>(), task_size);
}

namespace composition_polynomial {
namespace details {

template <typename FieldElementT>
class CompositionPolynomialImplWorkerMemory {
  using MultiplicativeNeighborsIterator = typename MultiplicativeNeighbors<FieldElementT>::Iterator;
  using PeriodicColumnIterator = typename PeriodicColumn<FieldElementT>::CosetEvaluation::Iterator;

 public:
  CompositionPolynomialImplWorkerMemory(
      size_t periodic_columns_size, size_t precomp_domain_evals_size, size_t batch_inverse_size)
      : periodic_column_vals(FieldElementT::UninitializedVector(periodic_columns_size)),
        precomp_domain_evals(FieldElementT::UninitializedVector(precomp_domain_evals_size)),
        batch_inverse_input(
            batch_inverse_size, FractionFieldElement<FieldElementT>::Uninitialized()),
        batch_inverse_output(FieldElementT::UninitializedVector(batch_inverse_size)) {
    periodic_columns_iter.reserve(periodic_columns_size);
  }

  std::vector<PeriodicColumnIterator> periodic_columns_iter;
  // Pre-allocated space for periodic column results.
  std::vector<FieldElementT> periodic_column_vals;

  // Pre-allocated vector of precomputed domain evaluations for each ConstraintsEval() invocation.
  std::vector<FieldElementT> precomp_domain_evals;

  // Pre-allocated space for batch_inverse.
  std::vector<FractionFieldElement<FieldElementT>> batch_inverse_input;
  std::vector<FieldElementT> batch_inverse_output;
};

}  // namespace details
}  // namespace composition_polynomial

template <typename AirT>
void CompositionPolynomialImpl<AirT>::EvalOnCosetBitReversedOutput(
    const FieldElementT& coset_offset,
    const MultiplicativeNeighbors<FieldElementT>& multiplicative_neighbors,
    gsl::span<FieldElementT> out_evaluation, uint64_t task_size) const {
  // Precompute useful constants.
  const size_t log_coset_size = SafeLog2(coset_size_);

  // Input verification.
  ASSERT_RELEASE(
      out_evaluation.size() == coset_size_,
      "Output span size does not match coset size: " + std::to_string(out_evaluation.size()) +
          " != " + std::to_string(coset_size_));

  ASSERT_RELEASE(
      multiplicative_neighbors.CosetSize() == coset_size_,
      "Given neighbor_iterator is not of expected length.");

  // Evaluate on coset.
  FieldElementT point = coset_offset;

  TaskManager& task_manager = TaskManager::GetInstance();
  std::vector<FieldElementT> algebraic_offsets;
  algebraic_offsets.reserve(DivCeil(coset_size_, task_size));

  FieldElementT point_multiplier = Pow(trace_generator_, task_size);
  for (uint64_t task_idx_offset = 0; task_idx_offset < coset_size_; task_idx_offset += task_size) {
    algebraic_offsets.push_back(point);
    point *= point_multiplier;
  }

  std::vector<std::vector<FieldElementT>> all_precomp_domain_evals =
      air_->PrecomputeDomainEvalsOnCoset(coset_offset, trace_generator_, point_exponents_, shifts_);
  std::vector<size_t> precomp_domain_masks;
  precomp_domain_masks.reserve(all_precomp_domain_evals.size());
  for (auto& vec : all_precomp_domain_evals) {
    precomp_domain_masks.push_back(vec.size() - 1);
  }

  using WorkerMemoryT =
      composition_polynomial::details::CompositionPolynomialImplWorkerMemory<FieldElementT>;
  std::vector<WorkerMemoryT> worker_mem;
  worker_mem.reserve(task_manager.GetNumThreads());
  for (size_t i = 0; i < task_manager.GetNumThreads(); ++i) {
    worker_mem.emplace_back(periodic_columns_.size(), all_precomp_domain_evals.size(), task_size);
  }

  std::vector<typename PeriodicColumn<FieldElementT>::CosetEvaluation> periodic_column_cosets;
  {
    ProfilingBlock periodic_block("Periodic columns computation.");

    periodic_column_cosets.reserve(periodic_columns_.size());
    for (const PeriodicColumn<FieldElementT>& column : periodic_columns_) {
      periodic_column_cosets.emplace_back(column.GetCoset(coset_offset, coset_size_));
    }
  }

  task_manager.ParallelFor(
      algebraic_offsets.size(),
      [this, &all_precomp_domain_evals, &precomp_domain_masks, &algebraic_offsets,
       &periodic_column_cosets, &worker_mem, &multiplicative_neighbors, &out_evaluation,
       log_coset_size, task_size](const TaskInfo& task_info) {
        uint64_t initial_point_idx = task_size * task_info.start_idx;
        auto point = algebraic_offsets[task_info.start_idx];
        WorkerMemoryT& wm = worker_mem[TaskManager::GetWorkerId()];

        wm.periodic_columns_iter.clear();
        for (const auto& column_coset : periodic_column_cosets) {
          wm.periodic_columns_iter.push_back(column_coset.begin() + initial_point_idx);
        }

        typename MultiplicativeNeighbors<FieldElementT>::Iterator neighbors_iter =
            multiplicative_neighbors.begin();
        neighbors_iter += static_cast<size_t>(initial_point_idx);

        const size_t actual_task_size = std::min(task_size, coset_size_ - initial_point_idx);
        const size_t end_of_coset_index = initial_point_idx + actual_task_size;

        for (size_t point_idx = initial_point_idx; point_idx < end_of_coset_index; point_idx++) {
          ASSERT_RELEASE(
              neighbors_iter != multiplicative_neighbors.end(),
              "neighbors_iter reached the end of the iterator unexpectedly");
          auto neighbors = *neighbors_iter;
          // Evaluate periodic columns.
          for (size_t i = 0; i < periodic_columns_.size(); ++i) {
            wm.periodic_column_vals[i] = *wm.periodic_columns_iter[i];
            ++wm.periodic_columns_iter[i];
          }
          for (size_t i = 0; i < all_precomp_domain_evals.size(); ++i) {
            wm.precomp_domain_evals[i] =
                all_precomp_domain_evals[i][point_idx & precomp_domain_masks[i]];
          }
          wm.batch_inverse_input[point_idx - initial_point_idx] = air_->ConstraintsEval(
              neighbors, wm.periodic_column_vals, coefficients_, point, shifts_,
              wm.precomp_domain_evals);

          // Advance evaluation point.
          point *= trace_generator_;
          ++neighbors_iter;
        }

        auto in_span = gsl::span<const FractionFieldElement<FieldElementT>>(wm.batch_inverse_input)
                           .first(actual_task_size);
        auto out_span = gsl::span<FieldElementT>(wm.batch_inverse_output).first(actual_task_size);
        FractionFieldElement<FieldElementT>::BatchToBaseFieldElement(in_span, out_span);

        for (size_t point_idx = initial_point_idx; point_idx < end_of_coset_index; point_idx++) {
          out_evaluation[BitReverse(point_idx, log_coset_size)] =
              wm.batch_inverse_output[point_idx - initial_point_idx];
        }
      });
}

}  // namespace starkware
