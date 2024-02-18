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

#include "starkware/air/boundary_constraints/boundary_periodic_column.h"

#include <algorithm>
#include <set>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using FieldElementT = TestFieldElement;

TEST(CreateBoundaryPeriodicColumn, Correctness) {
  Prng prng;

  const size_t n_values = 37;
  const uint64_t trace_length = 1024;
  const FieldElementT trace_generator = GetSubGroupGenerator<FieldElementT>(trace_length);
  const FieldElementT trace_offset = FieldElementT::RandomElement(&prng);

  const std::vector<uint64_t> rows =
      prng.UniformDistinctIntVector<uint64_t>(0, trace_length - 1, n_values);
  const std::vector<FieldElementT> values = prng.RandomFieldElementVector<FieldElementT>(n_values);

  const PeriodicColumn periodic_column = CreateBoundaryPeriodicColumn<FieldElementT>(
      rows, values, trace_length, trace_generator, trace_offset);
  const PeriodicColumn periodic_column_base = CreateBaseBoundaryPeriodicColumn<FieldElementT>(
      rows, trace_length, trace_generator, trace_offset);
  EXPECT_EQ(periodic_column.GetActualDegree(), static_cast<int64_t>(n_values) - 1);
  EXPECT_EQ(periodic_column_base.GetActualDegree(), static_cast<int64_t>(n_values) - 1);

  // Check that the ratio between the two periodic columns on the x values is the same as the ratio
  // between the requested y values.
  for (size_t i = 0; i < n_values; i++) {
    const FieldElementT x_value = trace_offset * Pow(trace_generator, rows[i]);
    const FieldElementT value = periodic_column.EvalAtPoint(x_value);
    const FieldElementT value_base = periodic_column_base.EvalAtPoint(x_value);
    ASSERT_EQ(value / value_base, values[i]) << "i = " << i;
  }

  // Make sure the following line doesn't throw an assert.
  periodic_column.GetCoset(FieldElementT::One(), trace_length);
}

TEST(CreateBoundaryPeriodicColumn, Empty) {
  Prng prng;
  const uint64_t trace_length = 1024;
  const FieldElementT trace_generator = GetSubGroupGenerator<FieldElementT>(trace_length);
  const FieldElementT trace_offset = FieldElementT::RandomElement(&prng);

  const PeriodicColumn periodic_column = CreateBoundaryPeriodicColumn<FieldElementT>(
      {}, {}, trace_length, trace_generator, trace_offset);
  const PeriodicColumn periodic_column_base = CreateBaseBoundaryPeriodicColumn<FieldElementT>(
      {}, trace_length, trace_generator, trace_offset);
  EXPECT_EQ(periodic_column.GetActualDegree(), -1);
  EXPECT_EQ(periodic_column_base.GetActualDegree(), -1);
  EXPECT_EQ(
      periodic_column.EvalAtPoint(FieldElementT::RandomElement(&prng)), FieldElementT::Zero());
  EXPECT_EQ(
      periodic_column_base.EvalAtPoint(FieldElementT::RandomElement(&prng)), FieldElementT::Zero());
}

TEST(CreateVanishingPeriodicColumn, Correctness) {
  Prng prng;

  const uint64_t trace_length = 1024;
  const uint64_t step = 4;
  const uint64_t coset_size = SafeDiv(trace_length, step);
  const FieldElementT trace_generator = GetSubGroupGenerator<FieldElementT>(trace_length);
  const FieldElementT trace_offset = FieldElementT::RandomElement(&prng);

  for (const uint64_t n_values : std::vector<uint64_t>{0, 31, 32, 33, coset_size}) {
    SCOPED_TRACE("n_values = " + std::to_string(n_values));

    // Choose n_values rows from the coset.
    std::vector<uint64_t> rows;
    if (n_values < coset_size) {
      rows = prng.UniformDistinctIntVector<uint64_t>(0, coset_size - 1, n_values);
    } else {
      ASSERT_EQ(n_values, coset_size);
      // UniformDistinctIntVector doesn't work in this case, just add all rows to the vector.
      rows.resize(coset_size);
      std::iota(rows.begin(), rows.end(), 0);
    }
    for (uint64_t& row : rows) {
      row *= step;
    }

    const std::set<uint64_t> rows_set(rows.begin(), rows.end());

    const PeriodicColumn periodic_column = CreateVanishingPeriodicColumn<FieldElementT>(
        rows, trace_length, trace_generator, trace_offset);
    const PeriodicColumn periodic_column_comp =
        CreateComplementVanishingPeriodicColumn<FieldElementT>(
            rows, step, trace_length, trace_generator, trace_offset);
    EXPECT_EQ(periodic_column.GetActualDegree(), static_cast<int64_t>(n_values));
    EXPECT_EQ(periodic_column_comp.GetActualDegree(), static_cast<int64_t>(coset_size - n_values));

    for (size_t i = 0; i < trace_length; i += step) {
      SCOPED_TRACE("i = " + std::to_string(i));
      const FieldElementT x_value = trace_offset * Pow(trace_generator, i);
      const FieldElementT value = periodic_column.EvalAtPoint(x_value);
      const FieldElementT value_comp = periodic_column_comp.EvalAtPoint(x_value);
      if (HasKey(rows_set, i)) {
        ASSERT_EQ(value, FieldElementT::Zero());
        ASSERT_NE(value_comp, FieldElementT::Zero());
      } else {
        ASSERT_NE(value, FieldElementT::Zero());
        ASSERT_EQ(value_comp, FieldElementT::Zero());
      }
    }

    // Make sure the following line doesn't throw an assert.
    periodic_column.GetCoset(FieldElementT::One(), trace_length);
    periodic_column_comp.GetCoset(FieldElementT::One(), trace_length);
  }
}

}  // namespace
}  // namespace starkware
