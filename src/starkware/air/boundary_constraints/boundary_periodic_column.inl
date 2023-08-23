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

#include "starkware/math/math.h"

namespace starkware {

namespace boundary_periodic_column {
namespace details {

/*
  Returns the field elements that correspond to the given rows (using the formula:
    trace_offset * trace_generator^row_index.
*/
template <typename FieldElementT>
std::vector<FieldElementT> RowIndicesToFieldElements(
    const gsl::span<const uint64_t> rows, const FieldElementT& trace_generator,
    const FieldElementT& trace_offset) {
  std::vector<FieldElementT> result;
  result.reserve(rows.size());
  for (const uint64_t row_index : rows) {
    result.push_back(trace_offset * Pow(trace_generator, row_index));
  }
  return result;
}

}  // namespace details
}  // namespace boundary_periodic_column

template <typename FieldElementT>
PeriodicColumn<FieldElementT> CreateBoundaryPeriodicColumn(
    const gsl::span<const uint64_t> rows, const gsl::span<const FieldElementT> values,
    const uint64_t trace_length, const FieldElementT& trace_generator,
    const FieldElementT& trace_offset) {
  ASSERT_RELEASE(rows.size() == values.size(), "Number of rows does not match number of values");
  const size_t n_values = rows.size();

  const std::vector<FieldElementT> x_values =
      boundary_periodic_column::details::RowIndicesToFieldElements(
          rows, trace_generator, trace_offset);

  // Find the minimal power of two which is >= n_values.
  const size_t log_column_height = n_values == 0 ? 1 : Log2Ceil(n_values);
  const uint64_t column_height = Pow2(log_column_height);
  // Find the generator of the subgroup of order column_height.
  const FieldElementT group_generator = Pow(trace_generator, SafeDiv(trace_length, column_height));

  std::vector<FieldElementT> reverse_cumulative_product =
      FieldElementT::UninitializedVector(n_values);
  std::vector<FieldElementT> periodic_column_values;
  periodic_column_values.reserve(column_height);
  const auto domain =
      MakeFftDomain(group_generator, log_column_height, FieldElementT::One(), false);

  for (const FieldElementT cur_x : domain) {
    // Fill reverse_cumulative_product with the values:
    // ..., (x - x[n-1])(x - x[n-2]), (x - x[n-1]), 1.
    FieldElementT prod = FieldElementT::One();
    for (size_t i = 0; i < n_values; ++i) {
      const size_t reverse_i = n_values - 1 - i;
      reverse_cumulative_product[reverse_i] = prod;
      prod *= cur_x - x_values[reverse_i];
    }

    // Compute sum_i(y[i] * prod_{j != i}(x - x_j)) by computing the cumulative product
    // (x - x[0])(x - x[1])...(x - x[i-1]) and using the values from reverse_cumulative_product.
    prod = FieldElementT::One();
    FieldElementT res = FieldElementT::Zero();
    for (size_t i = 0; i < x_values.size(); ++i) {
      res += values[i] * prod * reverse_cumulative_product[i];
      prod *= cur_x - x_values[i];
    }
    periodic_column_values.push_back(res);
  }

  return PeriodicColumn<FieldElementT>(
      periodic_column_values, trace_generator, FieldElementT::One(), trace_length,
      SafeDiv(trace_length, column_height));
}

template <typename FieldElementT>
PeriodicColumn<FieldElementT> CreateBaseBoundaryPeriodicColumn(
    const gsl::span<const uint64_t> rows, const uint64_t trace_length,
    const FieldElementT& trace_generator, const FieldElementT& trace_offset) {
  std::vector<FieldElementT> values(rows.size(), FieldElementT::One());
  return CreateBoundaryPeriodicColumn<FieldElementT>(
      rows, values, trace_length, trace_generator, trace_offset);
}

template <typename FieldElementT>
PeriodicColumn<FieldElementT> CreateVanishingPeriodicColumn(
    const gsl::span<const uint64_t> rows, const uint64_t trace_length,
    const FieldElementT& trace_generator, const FieldElementT& trace_offset) {
  std::vector<FieldElementT> x_values =
      boundary_periodic_column::details::RowIndicesToFieldElements(
          rows, trace_generator, trace_offset);

  // Find the minimal power of two which is > x_values.size().
  const size_t log_column_height = Log2Ceil(x_values.size() + 1);
  const uint64_t column_height = Pow2(log_column_height);
  // Find the generator of the subgroup of order column_height.
  const FieldElementT group_generator = Pow(trace_generator, SafeDiv(trace_length, column_height));

  std::vector<FieldElementT> periodic_column_values;
  periodic_column_values.reserve(column_height);
  const auto domain =
      MakeFftDomain(group_generator, log_column_height, FieldElementT::One(), false);

  for (const FieldElementT cur_x : domain) {
    FieldElementT res = FieldElementT::One();
    for (const FieldElementT& x_value : x_values) {
      res *= cur_x - x_value;
    }
    periodic_column_values.push_back(res);
  }

  return PeriodicColumn<FieldElementT>(
      periodic_column_values, trace_generator, FieldElementT::One(), trace_length,
      SafeDiv(trace_length, column_height));
}

template <typename FieldElementT>
PeriodicColumn<FieldElementT> CreateComplementVanishingPeriodicColumn(
    const gsl::span<const uint64_t> rows, const uint64_t step, const uint64_t trace_length,
    const FieldElementT& trace_generator, const FieldElementT& trace_offset) {
  const uint64_t coset_size = SafeDiv(trace_length, step);

  const std::set<uint64_t> rows_set(rows.begin(), rows.end());
  ASSERT_RELEASE(rows_set.size() == rows.size(), "Rows must be distinct");

  std::vector<uint64_t> other_rows;
  other_rows.reserve(coset_size - rows.size());
  for (size_t i = 0; i < trace_length; i += step) {
    if (!HasKey(rows_set, i)) {
      other_rows.push_back(i);
    }
  }
  ASSERT_RELEASE(other_rows.size() + rows.size() == coset_size, "All rows must be in the coset.");

  return CreateVanishingPeriodicColumn(other_rows, trace_length, trace_generator, trace_offset);
}

}  // namespace starkware
