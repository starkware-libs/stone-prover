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

#include "starkware/algebra/fft/multiplicative_fft.h"
#include "starkware/utils/bit_reversal.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

template <typename FieldElementT>
PeriodicColumn<FieldElementT>::PeriodicColumn(
    gsl::span<const FieldElementT> values, const FieldElementT& group_generator,
    const FieldElementT& offset, uint64_t coset_size, uint64_t column_step)
    : group_generator_(group_generator),
      column_step_(column_step),
      period_in_trace_(values.size() * column_step),
      n_copies_(SafeDiv(coset_size, period_in_trace_)),
      lde_manager_(
          MultiplicativeFftBases<FieldElementT, MultiplicativeGroupOrdering::kNaturalOrder>(
              Pow(group_generator, column_step * n_copies_), SafeLog2(values.size()),
              Pow(offset, n_copies_))) {
  lde_manager_.AddEvaluation(values);
}

template <typename FieldElementT>
FieldElementT PeriodicColumn<FieldElementT>::EvalAtPoint(const FieldElementT& x) const {
  std::vector<FieldElementT> points = {Pow(x, n_copies_)};
  std::vector<FieldElementT> outputs = FieldElementT::UninitializedVector(1);
  lde_manager_.EvalAtPoints(0, gsl::make_span(points), gsl::make_span(outputs));

  return outputs[0];
}

template <typename FieldElementT>
auto PeriodicColumn<FieldElementT>::GetCoset(
    const FieldElementT& start_point, const size_t coset_size) const -> CosetEvaluation {
  FieldElementT offset = Pow(start_point, n_copies_);
  const uint64_t n_values = lde_manager_.GetDomain(FieldElement(offset))->Size();
  ASSERT_RELEASE(
      coset_size == n_copies_ * column_step_ * n_values,
      "Currently coset_size must be the same as the size of the coset that was used to "
      "create the PeriodicColumn.");

  std::vector<FieldElementT> period_on_coset = FieldElementT::UninitializedVector(period_in_trace_);

  const size_t min_work_size = 1024;
  const FieldElementT offset_multiplier = Pow(group_generator_, n_copies_);
  TaskManager::GetInstance().ParallelFor(
      column_step_,
      [this, n_values, start_offset = offset, &offset_multiplier,
       &period_on_coset](const TaskInfo& task_info) {
        size_t start_idx = task_info.start_idx;

        FieldElementT offset = start_offset * Pow(offset_multiplier, start_idx);
        // Allocate storage for the LDE computation.
        std::vector<FieldElementT> lde = FieldElementT::UninitializedVector(n_values);
        std::array<FieldElementSpan, 1> output_spans{FieldElementSpan(gsl::make_span(lde))};
        for (size_t i = start_idx; i < task_info.end_idx; i++) {
          lde_manager_.EvalOnCoset(FieldElement(offset), output_spans);
          for (size_t j = 0; j < lde.size(); j++) {
            period_on_coset[i + j * column_step_] = lde[j];
          }
          offset *= offset_multiplier;
        }
      },
      column_step_, min_work_size / n_values);

  return CosetEvaluation(std::move(period_on_coset));
}

}  // namespace starkware
