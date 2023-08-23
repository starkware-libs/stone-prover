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

#include "starkware/algebra/polynomials.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/test_field_element.h"

namespace starkware {
namespace {

using testing::_;
using testing::ElementsAre;
using testing::Ne;

TEST(GetRandomPolynomial, BasicTest) {
  Prng prng;
  EXPECT_THAT(
      GetRandomPolynomial<TestFieldElement>(0, &prng), ElementsAre(Ne(TestFieldElement::Zero())));
  EXPECT_THAT(
      GetRandomPolynomial<TestFieldElement>(1, &prng),
      ElementsAre(_, Ne(TestFieldElement::Zero())));
  EXPECT_THAT(
      GetRandomPolynomial<TestFieldElement>(2, &prng),
      ElementsAre(_, _, Ne(TestFieldElement::Zero())));
}

TEST(HornerEvalBitReversed, Correctness) {
  Prng prng;
  const size_t degree = Pow2(prng.UniformInt(0, 4)) - 1;
  std::vector<TestFieldElement> coefs = GetRandomPolynomial<TestFieldElement>(degree, &prng);
  TestFieldElement x = TestFieldElement::RandomElement(&prng);
  EXPECT_EQ(HornerEval(x, coefs), HornerEvalBitReversed(x, BitReverseVector(coefs)));
}

TEST(BatchHornerEval, Correctness) {
  Prng prng;
  const size_t degree = Pow2(prng.UniformInt(0, 4)) - 1;
  std::vector<TestFieldElement> coefs = GetRandomPolynomial<TestFieldElement>(degree, &prng);

  const size_t n_points = prng.UniformInt(0, 10);
  std::vector<TestFieldElement> points = prng.RandomFieldElementVector<TestFieldElement>(n_points);
  std::vector<TestFieldElement> results(n_points, TestFieldElement::Zero());
  BatchHornerEval<TestFieldElement>(points, coefs, results);

  for (size_t i = 0; i < n_points; ++i) {
    ASSERT_EQ(results[i], HornerEval(points[i], coefs));
  }
}

TEST(ParallelBatchHornerEval, Correctness) {
  Prng prng;
  const size_t degree = Pow2(prng.UniformInt(0, 4)) - 1;
  const size_t stride = 4;
  auto interleaved_coefs = TestFieldElement::UninitializedVector((degree + 1) * stride);
  std::vector<std::vector<TestFieldElement>> coefs;
  for (size_t i = 0; i < stride; ++i) {
    auto poly = GetRandomPolynomial<TestFieldElement>(degree, &prng);
    coefs.push_back(poly);
    for (size_t j = 0; j < poly.size(); ++j) {
      interleaved_coefs[stride * j + i] = poly[j];
    }
  }

  const size_t n_points = prng.UniformInt(0, 10);
  std::vector<TestFieldElement> points = prng.RandomFieldElementVector<TestFieldElement>(n_points);
  std::vector<TestFieldElement> results(n_points * stride, TestFieldElement::Zero());

  ParallelBatchHornerEval<TestFieldElement>(
      points, interleaved_coefs, results, /*stride=*/stride, /*min_chunk_size=*/1);

  for (size_t i = 0; i < stride; ++i) {
    for (size_t j = 0; j < n_points; ++j) {
      ASSERT_EQ(results[stride * j + i], HornerEval(points[j], coefs[i]));
    }
  }
}

void TestOptimizedBatchHornerEval(
    const uint64_t degree_bound, uint64_t coset_size, size_t n_cosets,
    size_t n_points_not_in_cosets, Prng* prng) {
  const uint64_t degree = degree_bound - 1;
  std::vector<TestFieldElement> coefs = GetRandomPolynomial<TestFieldElement>(degree, prng);

  auto coset_generator = GetSubGroupGenerator<TestFieldElement>(coset_size);

  std::vector<TestFieldElement> points;
  const size_t n_points = n_cosets * coset_size + n_points_not_in_cosets;
  points.reserve(n_points);
  for (size_t i = 0; i < n_cosets; ++i) {
    auto point = TestFieldElement::RandomElement(prng);
    for (size_t j = 0; j < coset_size; ++j) {
      points.push_back(point);
      point *= coset_generator;
    }
  }
  for (size_t i = 0; i < n_points_not_in_cosets; ++i) {
    points.push_back(TestFieldElement::RandomElement(prng));
  }

  std::shuffle(points.begin(), points.end(), std::default_random_engine(0));

  std::vector<TestFieldElement> results(n_points, TestFieldElement::Zero());

  OptimizedBatchHornerEval<TestFieldElement>(points, coefs, results);

  for (size_t i = 0; i < n_points; ++i) {
    ASSERT_EQ(results[i], HornerEval(points[i], coefs));
  }
}

void TestOptimizedBatchHornerEval(const size_t log_degree) {
  Prng prng;
  const uint64_t coset_size = Pow2(prng.UniformInt(0, 4));
  const size_t n_cosets = prng.UniformInt(0, 20);
  const size_t n_points_not_in_cosets = prng.UniformInt(0, 20);
  TestOptimizedBatchHornerEval(
      /*degree_bound=*/Pow2(log_degree), /*coset_size=*/coset_size, /*n_cosets=*/n_cosets,
      /*n_points_not_in_cosets=*/n_points_not_in_cosets, &prng);
}

TEST(OptimizedBatchHornerEval, Correctness) {
  // Not optimized (small degree).
  TestOptimizedBatchHornerEval(0);
  TestOptimizedBatchHornerEval(9);
  // Optimized.
  TestOptimizedBatchHornerEval(11);
}

TEST(OptimizedBatchHornerEval, OneLargeCoset) {
  Prng prng;
  TestOptimizedBatchHornerEval(
      /*degree_bound=*/Pow2(10), /*coset_size=*/Pow2(11), /*n_cosets=*/1,
      /*n_points_not_in_cosets=*/0, &prng);
}

TEST(OptimizedBatchHornerEval, NonPowerOfTwo) {
  // Test the case where after one optimization round the degree bound becomes odd.
  Prng prng;
  TestOptimizedBatchHornerEval(
      /*degree_bound=*/2 * (1024 + 1), /*coset_size=*/8, /*n_cosets=*/1,
      /*n_points_not_in_cosets=*/0, &prng);
}

TEST(BatchHornerEvalBitReversed, Correctness) {
  Prng prng;
  const size_t degree = Pow2(prng.UniformInt(0, 4)) - 1;
  std::vector<TestFieldElement> coefs = GetRandomPolynomial<TestFieldElement>(degree, &prng);

  const size_t n_points = prng.UniformInt(0, 10);
  std::vector<TestFieldElement> points = prng.RandomFieldElementVector<TestFieldElement>(n_points);
  std::vector<TestFieldElement> results(n_points, TestFieldElement::Zero());
  BatchHornerEvalBitReversed<TestFieldElement>(points, coefs, results);

  for (size_t i = 0; i < n_points; ++i) {
    ASSERT_EQ(results[i], HornerEvalBitReversed(points[i], coefs));
  }
}

template <typename FieldElementT>
void TestLagrange(const std::vector<FieldElementT> poly, const size_t n_points, Prng* prng) {
  // As the interpolant has number of coefficients as the number of points it was interpolated from,
  // it is expected to have a suffix of zero coefficients.
  const std::vector<FieldElementT> expected_tail(n_points - poly.size(), FieldElementT::Zero());

  // Draw domain points.
  std::vector<FieldElementT> domain;
  while (domain.size() < n_points) {
    const FieldElementT point = FieldElementT::RandomElement(prng);
    if (std::count(domain.begin(), domain.end(), point) == 0) {
      domain.push_back(point);
    }
  }

  std::vector<FieldElementT> values = FieldElementT::UninitializedVector(domain.size());
  BatchHornerEval<TestFieldElement>(domain, poly, values);

  const std::vector<FieldElementT> interpolant =
      LagrangeInterpolation<FieldElementT>(domain, values);
  const auto interpolant_trimmed = gsl::make_span(interpolant).subspan(0, poly.size());
  const auto interpolant_tail = gsl::make_span(interpolant).subspan(poly.size());

  EXPECT_EQ(gsl::make_span(poly), interpolant_trimmed);
  EXPECT_EQ(gsl::make_span(expected_tail), interpolant_tail);
}

TEST(Polynomials, LagrangeInterpolationRandom) {
  using FieldElementT = TestFieldElement;
  Prng prng;

  // Initialize.
  const size_t degree = prng.UniformInt(0, 10);
  const size_t n_points = degree + prng.UniformInt(1, 10);
  const std::vector<FieldElementT> poly = GetRandomPolynomial<FieldElementT>(degree, &prng);

  // Test.
  TestLagrange<FieldElementT>(poly, n_points, &prng);
}

TEST(Polynomials, LagrangeInterpolationMinimalDomain) {
  using FieldElementT = TestFieldElement;
  Prng prng;

  // Initialize.
  const size_t degree = prng.UniformInt(0, 10);
  const size_t n_points = degree + 1;
  const std::vector<FieldElementT> poly = GetRandomPolynomial<FieldElementT>(degree, &prng);

  // Test.
  TestLagrange<FieldElementT>(poly, n_points, &prng);
}

TEST(Polynomials, LagrangeInterpolationZeroPoly) {
  using FieldElementT = TestFieldElement;
  Prng prng;

  // Initialize.
  const size_t n_points = prng.UniformInt(0, 10);
  const std::vector<FieldElementT> poly;

  // Test.
  TestLagrange<FieldElementT>(poly, n_points, &prng);
}

TEST(Polynomials, LagrangeInterpolationDegZeroPoly) {
  using FieldElementT = TestFieldElement;
  Prng prng;

  // Initialize.
  const size_t degree = 0;
  const size_t n_points = degree + prng.UniformInt(1, 10);
  const std::vector<FieldElementT> poly = GetRandomPolynomial<FieldElementT>(degree, &prng);

  // Test.
  TestLagrange<FieldElementT>(poly, n_points, &prng);
}

TEST(Polynomials, LagrangeInterpolationDegOnePoly) {
  using FieldElementT = TestFieldElement;
  Prng prng;

  // Initialize.
  const size_t degree = 1;
  const size_t n_points = degree + prng.UniformInt(1, 10);
  const std::vector<FieldElementT> poly = GetRandomPolynomial<FieldElementT>(degree, &prng);

  // Test.
  TestLagrange<FieldElementT>(poly, n_points, &prng);
}

}  // namespace
}  // namespace starkware
