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

#include "starkware/composition_polynomial/multiplicative_neighbors.h"

#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

using FieldElementT = TestFieldElement;

TEST(MultiplicativeNeighbors, Correctness) {
  const size_t trace_length = 8;
  const size_t n_columns = 4;
  const std::array<std::pair<int64_t, uint64_t>, 5> mask = {
      {{0, 0}, {0, 1}, {1, 2}, {2, 0}, {2, 3}}};
  std::vector<std::vector<FieldElementT>> trace;
  Prng prng;
  trace.reserve(n_columns);
  for (size_t i = 0; i < n_columns; ++i) {
    trace.push_back(prng.RandomFieldElementVector<FieldElementT>(trace_length));
  }
  MultiplicativeNeighbors<FieldElementT> neighbors(
      mask, std::vector<gsl::span<const FieldElementT>>(trace.begin(), trace.end()));
  std::vector<std::vector<FieldElementT>> result;
  for (auto vals : neighbors) {
    result.emplace_back(vals.begin(), vals.end());
  }
  EXPECT_EQ(
      result, std::vector<std::vector<FieldElementT>>({
                  {trace[0][0], trace[1][0], trace[2][1], trace[0][2], trace[3][2]},
                  {trace[0][1], trace[1][1], trace[2][2], trace[0][3], trace[3][3]},
                  {trace[0][2], trace[1][2], trace[2][3], trace[0][4], trace[3][4]},
                  {trace[0][3], trace[1][3], trace[2][4], trace[0][5], trace[3][5]},
                  {trace[0][4], trace[1][4], trace[2][5], trace[0][6], trace[3][6]},
                  {trace[0][5], trace[1][5], trace[2][6], trace[0][7], trace[3][7]},
                  {trace[0][6], trace[1][6], trace[2][7], trace[0][0], trace[3][0]},
                  {trace[0][7], trace[1][7], trace[2][0], trace[0][1], trace[3][1]},
              }));
}

TEST(MultiplicativeNeighbors, InvalidMask) {
  const size_t trace_length = 8;
  const size_t n_columns = 3;
  const std::array<std::pair<int64_t, uint64_t>, 5> mask = {
      {{0, 0}, {0, 1}, {1, 2}, {2, 0}, {2, 3}}};
  std::vector<std::vector<FieldElementT>> trace;
  Prng prng;
  trace.reserve(n_columns);
  for (size_t i = 0; i < n_columns; ++i) {
    trace.push_back(prng.RandomFieldElementVector<FieldElementT>(trace_length));
  }
  EXPECT_ASSERT(
      MultiplicativeNeighbors<FieldElementT>(
          mask, std::vector<gsl::span<const FieldElementT>>(trace.begin(), trace.end())),
      HasSubstr("Too few trace LDE columns provided"));
}

}  // namespace
}  // namespace starkware
