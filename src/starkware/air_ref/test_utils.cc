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

#include "starkware/air/test_utils.h"

#include <queue>
#include <vector>

#include "gtest/gtest.h"

#include "starkware/air/trace.h"
#include "starkware/algebra/domains/list_of_cosets.h"
#include "starkware/algebra/lde/lde.h"
#include "starkware/algebra/utils/invoke_template_version.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {

int64_t ComputeCompositionDegree(
    const Air& air, const Trace& trace, const ConstFieldElementSpan& random_coefficients,
    const size_t num_of_cosets) {
  ASSERT_RELEASE(
      (trace.Width() > 0) && (trace.GetColumn(0).Size() > 0), "Nothing to do with empty trace.");
  // Extract field.
  const Field field = trace.GetColumn(0)[0].GetField();

  // Evaluation domain specifications.
  const size_t coset_size = trace.GetColumn(0).Size();
  const size_t evaluation_domain_size =
      Pow2(Log2Ceil(air.GetCompositionPolynomialDegreeBound() * num_of_cosets));
  const size_t n_cosets = SafeDiv(evaluation_domain_size, coset_size);
  ListOfCosets domain(ListOfCosets::MakeListOfCosets(coset_size, n_cosets, field));
  auto cosets = domain.CosetsOffsets();

  // Initialize storage for trace LDE evaluations.
  std::unique_ptr<LdeManager> lde_manager = MakeLdeManager(domain.Bases());
  std::vector<FieldElementVector> trace_lde;
  trace_lde.reserve(trace.Width());
  for (size_t i = 0; i < trace.Width(); ++i) {
    lde_manager->AddEvaluation(trace.GetColumn(i));
    trace_lde.push_back(FieldElementVector::MakeUninitialized(field, coset_size));
  }

  // Construct composition polynomial.
  std::unique_ptr<CompositionPolynomial> composition_poly =
      air.CreateCompositionPolynomial(domain.TraceGenerator(), random_coefficients);

  // Evaluate composition.
  FieldElementVector evaluation =
      FieldElementVector::MakeUninitialized(field, evaluation_domain_size);
  for (size_t i = 0; i < n_cosets; ++i) {
    const FieldElement& coset_offset = cosets[BitReverse(i, SafeLog2(n_cosets))];
    lde_manager->EvalOnCoset(
        coset_offset, std::vector<FieldElementSpan>(trace_lde.begin(), trace_lde.end()));

    constexpr uint64_t kTaskSize = 256;

    composition_poly->EvalOnCosetBitReversedOutput(
        coset_offset, std::vector<ConstFieldElementSpan>(trace_lde.begin(), trace_lde.end()),
        evaluation.AsSpan().SubSpan(i * coset_size, coset_size), kTaskSize);
  }

  // Compute degree.
  auto group = MultiplicativeGroup::MakeGroup(evaluation_domain_size, field);
  auto lde_manager_eval = MakeBitReversedOrderLdeManager(group, group.GetField().One());
  lde_manager_eval->AddEvaluation(evaluation);

  return lde_manager_eval->GetEvaluationDegree(0);
}

Trace DrawRandomTrace(const size_t width, const size_t height, const Field& field, Prng* prng) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) {
        using FieldElementT = typename decltype(field_tag)::type;
        std::vector<std::vector<FieldElementT>> res;
        res.reserve(width);
        for (size_t i = 0; i < width; ++i) {
          res.push_back(prng->RandomFieldElementVector<FieldElementT>(height));
        }
        return Trace(std::move(res));
      },
      field);
}

bool TestOneConstraint(const Air& air, const Trace& trace, size_t constraint_id, Prng* prng) {
  ASSERT_RELEASE(
      trace.Width() > 0 && trace.GetColumn(0).Size() > 0, "Trace is expected to be not empty.");
  ASSERT_RELEASE(constraint_id < air.GetNumConstraints(), "Constraint id out of range.");

  // Constants.
  const Field field = trace.GetColumn(0)[0].GetField();
  FieldElementVector coefficients =
      FieldElementVector::Make(air.NumRandomCoefficients(), field.Zero());
  coefficients.Set(constraint_id, field.RandomElement(prng));
  const uint64_t degree = ComputeCompositionDegree(air, trace, coefficients);
  if (degree == uint64_t(-1)) {
    // Handle the special case of the zero polynomial.
    return true;
  }

  return degree < air.GetCompositionPolynomialDegreeBound();
}

/*
  Test if any of the constraints in the range [start, end) is failing.
*/
bool TestConstraintRange(const Air& air, const Trace& trace, size_t start, size_t end, Prng* prng) {
  ASSERT_RELEASE(
      trace.Width() > 0 && trace.GetColumn(0).Size() > 0, "Trace is expected to be not empty.");
  ASSERT_RELEASE(end > start, "Invalid range");
  ASSERT_RELEASE(start < air.GetNumConstraints(), "Constraint id out of range.");
  ASSERT_RELEASE(end <= air.GetNumConstraints(), "Constraint id out of range.");

  // Constants.
  const Field field = trace.GetColumn(0)[0].GetField();
  FieldElementVector coefficients =
      FieldElementVector::Make(air.NumRandomCoefficients(), field.Zero());
  for (size_t constraint_id = start; constraint_id < end; ++constraint_id) {
    coefficients.Set(constraint_id, field.RandomElement(prng));
  }
  const uint64_t degree = ComputeCompositionDegree(air, trace, coefficients);
  if (degree == uint64_t(-1)) {
    // Handle the special case of the zero polynomial.
    return true;
  }

  return degree < air.GetCompositionPolynomialDegreeBound();
}

std::set<size_t> GetFailingConstraints(const Air& air, const Trace& trace, Prng* prng) {
  const size_t n_constraints = air.GetNumConstraints();
  // Check unsatisfied constraints.
  std::set<size_t> result;
  for (size_t i = 0; i < n_constraints; ++i) {
    LOG(INFO) << "Testing constraint number " << i << ".";
    if (!TestOneConstraint(air, trace, i, prng)) {
      LOG(ERROR) << "Constraint " << i << " failed.";
      result.insert(i);
    }
  }

  return result;
}

std::set<size_t> GetFailingConstraintsBinarySearch(const Air& air, const Trace& trace, Prng* prng) {
  const size_t n_constraints = air.GetNumConstraints();
  // Initialize a queue of ranges, starting with the range [0, n_constraints).
  std::queue<std::pair<size_t, size_t>> queue;
  queue.emplace(0, n_constraints);

  std::set<size_t> result;
  while (!queue.empty()) {
    const auto [start, end] = queue.front();  // NOLINT: Structured binding syntax.
    queue.pop();
    LOG(INFO) << "Testing constraints in range [" << start << "," << end << ").";

    if (!TestConstraintRange(air, trace, start, end, prng)) {
      LOG(ERROR) << "Range [" << start << "," << end << ") failed.";
      // If the range is failing, break it into two ranges, and push them into the queue.
      if (end - start == 1) {
        result.insert(start);
      } else {
        const size_t m = (start + end) >> 1;
        queue.emplace(start, m);
        queue.emplace(m, end);
      }
    }
    // If the range is not failing, throw it away.
  }
  if (!result.empty()) {
    LOG(ERROR) << "Failing constraints: " << result;
  }
  return result;
}

void GetTraceRow(const Trace& trace, const size_t row_idx, const FieldElementSpan& dst) {
  ASSERT_RELEASE(dst.Size() == trace.Width(), "Span size must be equal to trace width.");
  for (size_t column = 0; column < trace.Width(); column++) {
    dst.Set(column, trace.GetColumn(column)[row_idx]);
  }
}

void SetTraceRow(Trace* trace, const size_t row_idx, const ConstFieldElementSpan& src) {
  ASSERT_RELEASE(src.Size() == trace->Width(), "Span size must be equal to trace width.");
  for (size_t column = 0; column < trace->Width(); column++) {
    trace->SetTraceElementForTesting(column, row_idx, src[column]);
  }
}

void ApplyManipulation(
    Trace* trace, const size_t row_idx,
    const std::function<void(FieldElementSpan curr_row, FieldElementSpan next_row)>&
        trace_manipulator) {
  ASSERT_RELEASE(trace != nullptr, "'trace' can not be nullptr.");
  ASSERT_RELEASE(
      (trace->Width() > 0) && (trace->GetColumn(0).Size() >= 2),
      "Trace expected to be non empty, and include at least 2 rows.");

  const Field field = trace->GetColumn(0).GetField();
  const size_t trace_length = trace->GetColumn(0).Size();

  // Fill the trace to satisfy constraints.
  FieldElementVector curr_row = FieldElementVector::MakeUninitialized(field, trace->Width());
  FieldElementVector next_row = FieldElementVector::MakeUninitialized(field, trace->Width());

  // Fetch current and next lines.
  GetTraceRow(*trace, row_idx, curr_row);
  GetTraceRow(*trace, (row_idx + 1) % trace_length, next_row);

  // Manipulate lines.
  trace_manipulator(curr_row, next_row);

  // Write back manipulated rows.
  SetTraceRow(trace, row_idx, curr_row);
  SetTraceRow(trace, (row_idx + 1) % trace_length, next_row);
}

void TestAirConstraint(
    const Air& air, const Field& field, const size_t trace_length, const size_t constraint_idx,
    const std::vector<size_t>& domain_indices,
    const std::function<void(FieldElementSpan curr_row, FieldElementSpan next_row, bool)>&
        trace_manipulator,
    Prng* prng) {
  // Construct random trace.
  Trace trace = DrawRandomTrace(air.NumColumns(), trace_length, field, prng);

  // Expect failure, as it is highly improbable the condition is satisfied by a random trace.
  EXPECT_FALSE(TestOneConstraint(air, trace, constraint_idx, prng));

  for (const size_t i : domain_indices) {
    // Manipulate lines.
    ApplyManipulation(&trace, i, [&](FieldElementSpan curr_row, FieldElementSpan next_row) {
      trace_manipulator(curr_row, next_row, true);
    });
  }

  // Verify constraint satisfied.
  EXPECT_TRUE(TestOneConstraint(air, trace, constraint_idx, prng));

  // Draw a row from the domain to ruin.
  const size_t bad_row_idx = domain_indices[prng->UniformInt(size_t(0), domain_indices.size() - 1)];

  // Change chosen row.
  ApplyManipulation(&trace, bad_row_idx, [&](FieldElementSpan curr_row, FieldElementSpan next_row) {
    trace_manipulator(curr_row, next_row, false);
  });

  // Expect constraints unsatisfied.
  EXPECT_FALSE(TestOneConstraint(air, trace, constraint_idx, prng))
      << "bad_row_idx = " << bad_row_idx;
}

std::vector<size_t> DomainPredicateToList(
    const std::function<bool(size_t)>& predicate, size_t n_elements) {
  std::vector<size_t> res;
  for (size_t i = 0; i < n_elements; ++i) {
    if (predicate(i)) {
      res.push_back(i);
    }
  }
  return res;
}

}  // namespace starkware
