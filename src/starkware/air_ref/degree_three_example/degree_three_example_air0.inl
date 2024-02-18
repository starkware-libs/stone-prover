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
DegreeThreeExampleAir<FieldElementT, 0>::CreateCompositionPolynomial(
    const FieldElement& trace_generator, const ConstFieldElementSpan& random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const FieldElementT& gen = trace_generator.As<FieldElementT>();

  const std::vector<uint64_t> point_exponents = {trace_length_};
  const std::vector<uint64_t> gen_exponents = {(trace_length_) - (1), res_claim_index_};

  BuildPeriodicColumns(gen, &builder);

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
DegreeThreeExampleAir<FieldElementT, 0>::PrecomputeDomainEvalsOnCoset(
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
FractionFieldElement<FieldElementT> DegreeThreeExampleAir<FieldElementT, 0>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, [[maybe_unused]] const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 2, "shifts should contain 2 elements.");

  // domain0 = point^trace_length - 1.
  const FieldElementT& domain0 = precomp_domains[0];
  // domain1 = point - gen^(trace_length - 1).
  const FieldElementT& domain1 = (point) - (shifts[0]);
  // domain2 = point - gen^res_claim_index.
  const FieldElementT& domain2 = (point) - (shifts[1]);

  ASSERT_VERIFIER(neighbors.size() == 2, "Neighbors must contain 2 elements.");
  const FieldElementT& x_row0 = neighbors[kXRow0Neighbor];
  const FieldElementT& x_row1 = neighbors[kXRow1Neighbor];

  ASSERT_VERIFIER(periodic_columns.size() == 1, "periodic_columns should contain 1 elements.");
  const FieldElementT& add_three = periodic_columns[kAddThreePeriodicColumn];

  FractionFieldElement<FieldElementT> res(FieldElementT::Zero());
  {
    // Compute a sum of constraints with denominator = domain0.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain1.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for step:
        const FieldElementT constraint =
            (((((FieldElementT::ConstexprFromBigInt(0x10_Z)) * (x_row0)) * (x_row0)) * (x_row0)) +
             (add_three)) -
            (x_row1);
        inner_sum += random_coefficients[0] * constraint;
      }
      outer_sum += inner_sum * domain1;
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
        // Constraint expression for verify_res:
        const FieldElementT constraint = (x_row0) - (claimed_res_);
        inner_sum += random_coefficients[1] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain2);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> DegreeThreeExampleAir<FieldElementT, 0>::DomainEvalsAtPoint(
    gsl::span<const FieldElementT> point_powers,
    [[maybe_unused]] gsl::span<const FieldElementT> shifts) const {
  const FieldElementT& domain0 = (point_powers[1]) - (FieldElementT::One());
  return {
      domain0,
  };
}

template <typename FieldElementT>
TraceGenerationContext DegreeThreeExampleAir<FieldElementT, 0>::GetTraceGenerationContext() const {
  TraceGenerationContext ctx;

  ASSERT_RELEASE((1) <= (trace_length_), "step must not exceed dimension.");

  ASSERT_RELEASE((trace_length_) <= (trace_length_), "Index out of range.");

  ASSERT_RELEASE((trace_length_) >= (0), "Index should be non negative.");

  ctx.AddPeriodicColumn(
      "add_three", VirtualColumn(/*column=*/kAddThreePeriodicColumn, /*step=*/1, /*row_offset=*/0));

  return ctx;
}

template <typename FieldElementT>
std::vector<std::pair<int64_t, uint64_t>> DegreeThreeExampleAir<FieldElementT, 0>::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;

  mask.reserve(2);
  mask.emplace_back(0, kXColumn);
  mask.emplace_back(1, kXColumn);

  return mask;
}

}  // namespace starkware
