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

#ifndef STARKWARE_AIR_TEST_UTILS_H_
#define STARKWARE_AIR_TEST_UTILS_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/trace.h"
#include "starkware/air/trace_context.h"
#include "starkware/math/math.h"

namespace starkware {

/*
  Flexible AIR class used for tests.
*/
template <typename FieldElementT>
class DummyAir : public Air {
 public:
  using FieldElementT_ = FieldElementT;

  explicit DummyAir(uint64_t trace_length) : Air(trace_length) {}

  uint64_t GetCompositionPolynomialDegreeBound() const override {
    ASSERT_RELEASE(
        composition_polynomial_degree_bound,
        "composition_polynomial_degree_bound wasn't initialized.");

    return *composition_polynomial_degree_bound;
  }

  uint64_t NumRandomCoefficients() const override { return n_constraints; }

  uint64_t NumColumns() const override { return n_columns; }

  std::optional<InteractionParams> GetInteractionParams() const override { return std::nullopt; }

  std::vector<std::vector<FieldElementT>> PrecomputeDomainEvalsOnCoset(
      const FieldElementT& point, const FieldElementT& generator,
      gsl::span<const uint64_t> point_exponents,
      gsl::span<const FieldElementT> /* shifts */) const {
    // domain0 = point^(first_exponent) - 1.
    uint64_t size = SafeDiv(trace_length_, point_exponents[0]);
    std::vector<FieldElementT> point_powers;
    point_powers.reserve(size);
    auto power = Pow(point, point_exponents[0]);
    const FieldElementT gen_power = Pow(generator, point_exponents[0]);
    point_powers.push_back(power);
    for (size_t j = 1; j < size; ++j) {
      power *= gen_power;
      point_powers.push_back(power);
    }

    std::vector<std::vector<FieldElementT>> precomp_domains(1);
    precomp_domains[0].reserve(size);

    for (uint64_t i = 0; i < size; ++i) {
      precomp_domains[0].push_back(point_powers[i] - FieldElementT::One());
    }
    return precomp_domains;
  }

  auto ConstraintsEval(
      gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
      gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
      gsl::span<const FieldElementT> gen_powers,
      gsl::span<const FieldElementT> precomp_domains) const {
    ASSERT_RELEASE(random_coefficients.size() == constraints.size(), "This is a bug in the test.");
    FractionFieldElement<FieldElementT> res(FieldElementT::Zero());
    for (size_t i = 0; i < constraints.size(); ++i) {
      res += constraints[i](
          neighbors, periodic_columns, random_coefficients, point, gen_powers, precomp_domains);
    }
    return res;
  }

  std::vector<FieldElementT> DomainEvalsAtPoint(
      gsl::span<const FieldElementT> point_powers,
      gsl::span<const FieldElementT> /* shifts */) const {
    if (point_powers.size() <= 1) {
      return {};
    }
    return {point_powers[1] - FieldElementT::One()};
  }

  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const FieldElement& trace_generator,
      const ConstFieldElementSpan& random_coefficients) const override {
    typename CompositionPolynomialImpl<DummyAir>::Builder builder(periodic_columns.size());

    const FieldElementT& gen = trace_generator.As<FieldElementT>();

    for (size_t i = 0; i < periodic_columns.size(); ++i) {
      builder.AddPeriodicColumn(periodic_columns[i], i);
    }

    return builder.BuildUniquePtr(
        UseOwned(this), gen, TraceLength(), random_coefficients.As<FieldElementT>(),
        point_exponents, BatchPow(gen, gen_exponents));
  }

  /*
    This is a helper function for tests that don't want to specify a generator.
  */
  std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
      const ConstFieldElementSpan& random_coefficients) const {
    return CreateCompositionPolynomial(
        FieldElement(GetSubGroupGenerator<FieldElementT>(TraceLength())), random_coefficients);
  }

  std::vector<std::pair<int64_t, uint64_t>> GetMask() const override { return mask; }

  size_t n_constraints = 0;
  size_t n_columns = 0;
  std::vector<std::pair<int64_t, uint64_t>> mask;

  std::vector<PeriodicColumn<FieldElementT>> periodic_columns;
  std::vector<uint64_t> point_exponents;
  std::vector<uint64_t> gen_exponents;
  std::vector<std::function<FractionFieldElement<FieldElementT>(
      gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
      gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
      gsl::span<const FieldElementT> gen_powers, gsl::span<const FieldElementT> precomp_evals)>>
      constraints;

  /*
    If the value is nullopt, GetCompositionPolynomialDegreeBound will fail.
  */
  std::optional<uint64_t> composition_polynomial_degree_bound = std::nullopt;
};

/*
  Returns the degree of applying the air constraints, given the provided random coefficients, on
  the provided trace. Used for air-constraints unit testing. This function assumes the random
  coefficients are used only to bind constraints together, meaning, the number of constraints is
  exactly half the number of random coefficients, and the composition polynomial is of the form:
  \sum constraint_i(x) * (coeff_{2*i} + coeff_{2*i+1} * x^{n_i}).
*/
int64_t ComputeCompositionDegree(
    const Air& air, const Trace& trace, const ConstFieldElementSpan& random_coefficients,
    size_t num_of_cosets = 2);
/*
  Tests if the given constraint (with the provided id, from given AIR instance) is satisfied by
  provided trace.
  Technically, it uses random coefficients vector with only coefficients # 2*constraint_id and
  2*constraint_id+1 set randomly, and the rest are all zeros, and returns true if and only if the
  composition degree is lower than the bound.
  In particular, this function assumes the random coefficients are used only to bind constraints
  together, so that the i'th constraint is verified using the coefficients from indices 2*i,
  2*i+1.
*/
bool TestOneConstraint(const Air& air, const Trace& trace, size_t constraint_id, Prng* prng);

/*
  Returns the list of constraints not satisfied by given trace.

  This function assumes the random coefficients are used only to bind constraints
  together, so that the i'th constraint is verified using the coefficients from indices 2*i,
  2*i+1.
*/
std::set<size_t> GetFailingConstraints(const Air& air, const Trace& trace, Prng* prng);

/*
  Similar to GetFailingConstraints(), but uses a binary search on intervals, to find the set
  of failing constraints.
*/
std::set<size_t> GetFailingConstraintsBinarySearch(const Air& air, const Trace& trace, Prng* prng);

Trace DrawRandomTrace(size_t width, size_t height, const Field& field, Prng* prng);

/*
  A general function to verify constraint satisfiability.
  (currently supports only constraints accessing at most two consecutive rows).

  General test flow:
  1) Draw a random trace, and verify constraint is not satisfied on it.
  2) Locally fix the trace, by passing pairs of consecutive lines to the trace_manipulator (given
  true as last parameter), and verify manipulated trace satisfies constraint.
  3) Ruin the trace by changing one of the manipulated lines, using the trace_manipulator with
  'false' as it's last parameter, and verify the ruined trace does not satisfy the constraint.

  Parameters:
  - air, field, trace_length, and prng are straight forward.
  - constraint_idx is the index of the constraint to test (defined by the AIR).
  - domain_indices is a list of indices from the range 0 <= i < trace_length.
  Index 'i' is in the list if the constraint satisfaction is affected by the values in rows
  'i','i+1 % trace_length'.
  - trace_manipulator is a function changing the values of two consecutive rows, and if the last
  parameter is true it is expected to change the rows so the constraint can be satisfied, and if
  false, the row values are changed so the constraint is not satisfied.
  The rows passed to the manipulator in the order they appear in domain_indices.
*/
void TestAirConstraint(
    const Air& air, const Field& field, size_t trace_length, size_t constraint_idx,
    const std::vector<size_t>& domain_indices,
    const std::function<void(FieldElementSpan curr_row, FieldElementSpan next_row, bool)>&
        trace_manipulator,
    Prng* prng);

/*
  Returns an ordered list of integers from the range 0 <= n < n_elements such that
  predicate(n) = true.
*/
std::vector<size_t> DomainPredicateToList(
    const std::function<bool(size_t)>& predicate, size_t n_elements);

/*
  Given a span of Traces, returns a merged trace. Original traces are invalidated in the process.
*/
template <typename FieldElementT>
Trace MergeTraces(const gsl::span<Trace> traces) {
  std::vector<std::vector<FieldElementT>> merged_trace_vals;
  size_t merged_trace_size = 0;
  for (const auto& trace : traces) {
    merged_trace_size += trace.Width();
  }
  merged_trace_vals.reserve(merged_trace_size);

  for (auto& trace : traces) {
    std::vector<FieldElementVector> columns = std::move(trace).ConsumeAsColumnsVector();
    for (FieldElementVector& column : columns) {
      merged_trace_vals.push_back(std::move(column.As<FieldElementT>()));
    }
  }
  return Trace(std::move(merged_trace_vals));
}

/*
  Test helper class generates a trace context and initializes it with a given trace.
*/
class TestTraceContext : public TraceContext {
 public:
  explicit TestTraceContext(Trace trace) : trace_(std::make_unique<Trace>(std::move(trace))) {}

  Trace GetTrace() override {
    ASSERT_RELEASE(!get_trace_was_called_, "GetTrace of TestTraceContext was called twice.");
    get_trace_was_called_ = true;
    return std::move(*trace_);
  }

  Trace GetInteractionTrace() override {
    ASSERT_RELEASE(false, "Calling GetInteractionTrace from test.");
  }

 private:
  std::unique_ptr<Trace> trace_;
  bool get_trace_was_called_ = false;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_TEST_UTILS_H_
