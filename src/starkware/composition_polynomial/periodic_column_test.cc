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

#include "starkware/composition_polynomial/periodic_column.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

using FieldElementT = TestFieldElement;

/*
  This tests creates a PeriodicColumn with the given parameters, and performs the following tests:
    1. Evaluate and GetIterator on the same coset returns the original values.
    2. Evaluate on a random point and GetIterator on a random cosets are consistent with the
       interpolation polynomial computed using LagrangeInterpolation().
*/
void TestPeriodicColumn(
    const size_t n_values, const size_t coset_size, const size_t column_step, Prng* prng) {
  SCOPED_TRACE(
      "n_values=" + std::to_string(n_values) + ", coset_size=" + std::to_string(coset_size) +
      ", column_step=" + std::to_string(column_step));
  FieldElementT group_generator = GetSubGroupGenerator<FieldElementT>(coset_size);
  FieldElementT offset = FieldElementT::RandomElement(prng);
  std::vector<FieldElementT> values = prng->RandomFieldElementVector<FieldElementT>(n_values);
  PeriodicColumn<FieldElementT> column(values, group_generator, offset, coset_size, column_step);

  FieldElementT point = offset;
  std::vector<FieldElementT> domain;
  std::vector<FieldElementT> values_ext;  // Contains coset_size/n_values repetitions of values.
  auto periodic_column_coset = column.GetCoset(offset, coset_size);
  auto it = periodic_column_coset.begin();
  for (size_t i = 0; i < coset_size; ++i) {
    SCOPED_TRACE("i = " + std::to_string(i));

    const FieldElementT iterator_value = *it;

    // Check that the iterator agrees with EvalAtPoint.
    ASSERT_EQ(iterator_value, column.EvalAtPoint(point));

    // If this row belongs to the virtual column, compare with the expected value.
    if (i % column_step == 0) {
      const FieldElementT expected_val = values[(i / column_step) % values.size()];
      ASSERT_EQ(expected_val, iterator_value);
    }

    domain.push_back(point);
    values_ext.push_back(iterator_value);
    point *= group_generator;
    ++it;
  }

  // Check that this is indeed an extrapolation of a low-degree polynomial, by computing the
  // expected polynomial and substituting a random point.
  std::vector<FieldElementT> interpolant = LagrangeInterpolation<FieldElementT>(domain, values_ext);
  FieldElementT random_point = FieldElementT::RandomElement(prng);
  EXPECT_EQ(HornerEval(random_point, interpolant), column.EvalAtPoint(random_point));

  // Check the iterator on a coset of a possibly different size.
  point = random_point;
  auto periodic_column_coset2 = column.GetCoset(random_point, coset_size);
  auto it2 = periodic_column_coset2.begin();
  for (size_t i = 0; i < coset_size; ++i) {
    ASSERT_EQ(HornerEval(point, interpolant), *it2);
    point *= group_generator;
    ++it2;
  }
}

TEST(PeriodicColumn, Correctness) {
  Prng prng;
  TestPeriodicColumn(8, 32, 1, &prng);
  TestPeriodicColumn(8, 32, 2, &prng);
  TestPeriodicColumn(8, 32, 4, &prng);
  TestPeriodicColumn(8, 8, 1, &prng);
  TestPeriodicColumn(1, 8, 1, &prng);
  TestPeriodicColumn(1, 8, 8, &prng);

  // Random sizes.
  const size_t log_coset_size = prng.UniformInt(0, 5);
  const auto log_n_values = prng.template UniformInt<size_t>(0, log_coset_size);
  const auto log_column_step = prng.template UniformInt<size_t>(0, log_coset_size - log_n_values);
  TestPeriodicColumn(Pow2(log_n_values), Pow2(log_coset_size), Pow2(log_column_step), &prng);
}

TEST(PeriodicColumn, NumberOfValuesMustDivideCosetSize) {
  Prng prng;
  EXPECT_ASSERT(TestPeriodicColumn(3, 32, 1, &prng), HasSubstr("doesn't divide the numerator"));
}

TEST(PeriodicColumn, InvalidCosetSize) {
  Prng prng;
  EXPECT_ASSERT(TestPeriodicColumn(3, 12, 1, &prng), HasSubstr("must be a power of 2"));
}

}  // namespace
}  // namespace starkware
