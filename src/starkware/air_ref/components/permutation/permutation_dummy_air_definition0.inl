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

#include <map>
#include <utility>

#include "starkware/math/math.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

template <typename FieldElementT>
std::unique_ptr<CompositionPolynomial>
PermutationDummyAirDefinition<FieldElementT, 0>::CreateCompositionPolynomial(
    const FieldElement& trace_generator, const ConstFieldElementSpan& random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const FieldElementT& gen = trace_generator.As<FieldElementT>();

  const std::vector<uint64_t> point_exponents = {trace_length_};
  const std::vector<uint64_t> gen_exponents = {(trace_length_) - (1)};

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
PermutationDummyAirDefinition<FieldElementT, 0>::PrecomputeDomainEvalsOnCoset(
    const FieldElementT& point, const FieldElementT& generator,
    gsl::span<const uint64_t> point_exponents,
    [[maybe_unused]] gsl::span<const FieldElementT> shifts) const {
  const std::vector<FieldElementT> strict_point_powers = BatchPow(point, point_exponents);
  const std::vector<FieldElementT> gen_powers = BatchPow(generator, point_exponents);

  // point_powers[i][j] is the evaluation of the ith power at its jth point.
  // The index j runs until the order of the domain (beyond we'd cycle back to point_powers[i][0]).
  std::vector<std::vector<FieldElementT>> point_powers(point_exponents.size());
  for (size_t i = 0; i < point_exponents.size(); ++i) {
    uint64_t size = SafeDiv(trace_length_, point_exponents[i]);
    auto& vec = point_powers[i];
    auto power = strict_point_powers[i];
    vec.reserve(size);
    vec.push_back(power);
    for (size_t j = 1; j < size; ++j) {
      power *= gen_powers[i];
      vec.push_back(power);
    }
  }

  TaskManager& task_manager = TaskManager::GetInstance();
  constexpr size_t kPeriodUpperBound = 524289;
  constexpr size_t kTaskSize = 1024;
  size_t period;

  std::vector<std::vector<FieldElementT>> precomp_domains = {
      FieldElementT::UninitializedVector(1),
  };

  period = 1;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[0][i] = (point_powers[0][i & (0)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);
  return precomp_domains;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT>
PermutationDummyAirDefinition<FieldElementT, 0>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, [[maybe_unused]] const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 1, "shifts should contain 1 elements.");

  // domain0 = point^trace_length - 1.
  const FieldElementT& domain0 = precomp_domains[0];
  // domain1 = point - 1.
  const FieldElementT& domain1 = (point) - (FieldElementT::One());
  // domain2 = point - gen^(trace_length - 1).
  const FieldElementT& domain2 = (point) - (shifts[0]);

  ASSERT_VERIFIER(neighbors.size() == 14, "Neighbors must contain 14 elements.");
  const FieldElementT& column0_row0 = neighbors[kColumn0Row0Neighbor];
  const FieldElementT& column0_row1 = neighbors[kColumn0Row1Neighbor];
  const FieldElementT& column1_row0 = neighbors[kColumn1Row0Neighbor];
  const FieldElementT& column1_row1 = neighbors[kColumn1Row1Neighbor];
  const FieldElementT& column2_row0 = neighbors[kColumn2Row0Neighbor];
  const FieldElementT& column2_row1 = neighbors[kColumn2Row1Neighbor];
  const FieldElementT& column3_row0 = neighbors[kColumn3Row0Neighbor];
  const FieldElementT& column3_row1 = neighbors[kColumn3Row1Neighbor];
  const FieldElementT& column4_row0 = neighbors[kColumn4Row0Neighbor];
  const FieldElementT& column4_row1 = neighbors[kColumn4Row1Neighbor];
  const FieldElementT& column5_row0 = neighbors[kColumn5Row0Neighbor];
  const FieldElementT& column5_row1 = neighbors[kColumn5Row1Neighbor];
  const FieldElementT& column6_inter1_row0 = neighbors[kColumn6Inter1Row0Neighbor];
  const FieldElementT& column6_inter1_row1 = neighbors[kColumn6Inter1Row1Neighbor];

  ASSERT_VERIFIER(periodic_columns.empty(), "periodic_columns should be empty.");

  FractionFieldElement<FieldElementT> res(FieldElementT::Zero());
  {
    // Compute a sum of constraints with denominator = domain1.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for multi_column_perm/perm/init0:
        const FieldElementT constraint =
            ((((((multi_column_perm__perm__interaction_elm_) -
                 (((column3_row0) +
                   ((multi_column_perm__hash_interaction_elm0_) * (column4_row0))) +
                  ((multi_column_perm__hash_interaction_elm1_) * (column5_row0)))) *
                (column6_inter1_row0)) +
               (column0_row0)) +
              ((multi_column_perm__hash_interaction_elm0_) * (column1_row0))) +
             ((multi_column_perm__hash_interaction_elm1_) * (column2_row0))) -
            (multi_column_perm__perm__interaction_elm_);
        inner_sum += random_coefficients[0] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain1);
  }

  {
    // Compute a sum of constraints with denominator = domain0.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain2.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for multi_column_perm/perm/step0:
        const FieldElementT constraint =
            (((multi_column_perm__perm__interaction_elm_) -
              (((column3_row1) + ((multi_column_perm__hash_interaction_elm0_) * (column4_row1))) +
               ((multi_column_perm__hash_interaction_elm1_) * (column5_row1)))) *
             (column6_inter1_row1)) -
            (((multi_column_perm__perm__interaction_elm_) -
              (((column0_row1) + ((multi_column_perm__hash_interaction_elm0_) * (column1_row1))) +
               ((multi_column_perm__hash_interaction_elm1_) * (column2_row1)))) *
             (column6_inter1_row0));
        inner_sum += random_coefficients[1] * constraint;
      }
      outer_sum += inner_sum * domain2;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain0);
  }

  {
    // Compute a sum of constraints with denominator = domain2.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for multi_column_perm/perm/last:
        const FieldElementT constraint =
            (column6_inter1_row0) - (multi_column_perm__perm__public_memory_prod_);
        inner_sum += random_coefficients[2] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain2);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> PermutationDummyAirDefinition<FieldElementT, 0>::DomainEvalsAtPoint(
    gsl::span<const FieldElementT> point_powers,
    [[maybe_unused]] gsl::span<const FieldElementT> shifts) const {
  const FieldElementT& domain0 = (point_powers[1]) - (FieldElementT::One());
  return {
      domain0,
  };
}

template <typename FieldElementT>
TraceGenerationContext PermutationDummyAirDefinition<FieldElementT, 0>::GetTraceGenerationContext()
    const {
  TraceGenerationContext ctx;

  ASSERT_RELEASE(((trace_length_) + (-1)) < (trace_length_), "Index out of range.");

  ASSERT_RELEASE(((trace_length_) + (-1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((1) <= (trace_length_), "step must not exceed dimension.");

  ASSERT_RELEASE(((trace_length_) - (1)) <= (trace_length_), "Index out of range.");

  ASSERT_RELEASE(((trace_length_) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (trace_length_), "Index out of range.");

  ASSERT_RELEASE((0) <= ((trace_length_) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE((trace_length_) <= (trace_length_), "Index out of range.");

  ASSERT_RELEASE((trace_length_) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (trace_length_), "Index out of range.");

  ctx.AddVirtualColumn(
      "original0", VirtualColumn(/*column=*/kColumn0Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "original1", VirtualColumn(/*column=*/kColumn1Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "original2", VirtualColumn(/*column=*/kColumn2Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "perm0", VirtualColumn(/*column=*/kColumn3Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "perm1", VirtualColumn(/*column=*/kColumn4Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "perm2", VirtualColumn(/*column=*/kColumn5Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "multi_column_perm/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn6Inter1Column - kNumColumnsFirst, /*step=*/1, /*row_offset=*/0));

  return ctx;
}

template <typename FieldElementT>
std::vector<std::pair<int64_t, uint64_t>> PermutationDummyAirDefinition<FieldElementT, 0>::GetMask()
    const {
  std::vector<std::pair<int64_t, uint64_t>> mask;

  mask.reserve(14);
  mask.emplace_back(0, kColumn0Column);
  mask.emplace_back(1, kColumn0Column);
  mask.emplace_back(0, kColumn1Column);
  mask.emplace_back(1, kColumn1Column);
  mask.emplace_back(0, kColumn2Column);
  mask.emplace_back(1, kColumn2Column);
  mask.emplace_back(0, kColumn3Column);
  mask.emplace_back(1, kColumn3Column);
  mask.emplace_back(0, kColumn4Column);
  mask.emplace_back(1, kColumn4Column);
  mask.emplace_back(0, kColumn5Column);
  mask.emplace_back(1, kColumn5Column);
  mask.emplace_back(0, kColumn6Inter1Column);
  mask.emplace_back(1, kColumn6Inter1Column);

  return mask;
}

}  // namespace starkware
