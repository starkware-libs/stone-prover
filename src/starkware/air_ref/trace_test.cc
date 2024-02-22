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

#include "starkware/air/trace.h"

#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using FieldElementT = TestFieldElement;

TEST(Trace, Getters) {
  Prng prng;
  const size_t width = prng.UniformInt(1, 10);
  const size_t height = prng.UniformInt(1, 10);

  // Construct trace values.
  std::vector<std::vector<FieldElementT>> trace_vals =
      Trace::Allocate<FieldElementT>(width, height);

  // Keep trace values for future comparison.
  const std::vector<std::vector<FieldElementT>> trace_vals_saved = trace_vals;

  // Construct trace.
  Trace trace(std::move(trace_vals));

  // Compare values with different getters.
  const std::vector<gsl::span<const FieldElementT>> trace_as = trace.As<FieldElementT>();
  EXPECT_EQ(trace_as.size(), width);
  for (size_t i = 0; i < width; ++i) {
    EXPECT_EQ(trace_as[i].size(), height);
    EXPECT_EQ(trace.GetColumn(i).Size(), height);
    for (size_t j = 0; j < height; j++) {
      EXPECT_EQ(trace_as[i][j], trace_vals_saved[i][j]);
      EXPECT_EQ(trace.GetColumn(i)[j].As<FieldElementT>(), trace_vals_saved[i][j]);
    }
  }
}

}  // namespace
}  // namespace starkware
