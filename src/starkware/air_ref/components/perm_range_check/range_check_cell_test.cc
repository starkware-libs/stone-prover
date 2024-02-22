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

#include "starkware/air/components/perm_range_check/range_check_cell.h"

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/components/trace_generation_context.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::HasSubstr;

using FieldElementT = TestFieldElement;

class RangeCheckCellTest : public ::testing::Test {
 public:
  const uint64_t trace_length = 256;
  const uint64_t values_length = 60;
  TraceGenerationContext ctx;

  RangeCheckCellTest() {
    ctx.AddVirtualColumn("test", VirtualColumn(/*column=*/0, /*step=*/1, /*row_offset=*/0));
  }
};

/*
  Tests that Finalize() fills holes correctly.
*/
TEST_F(RangeCheckCellTest, WriteTraceTest) {
  RangeCheckCell<FieldElementT> rc_cell("test", ctx, trace_length);
  Prng prng;
  const auto indices = prng.UniformDistinctIntVector<uint64_t>(0, trace_length - 1, values_length);
  const uint64_t minimum = prng.UniformInt<uint64_t>(0, 1000);
  const uint64_t maximum = minimum + trace_length - values_length - 1;
  const auto values = prng.UniformIntVector<uint64_t>(minimum, maximum, values_length);

  // Initialize a single column trace.
  std::vector<std::vector<FieldElementT>> trace = {
      std::vector<FieldElementT>(trace_length, FieldElementT::Zero())};
  for (size_t i = 0; i < values_length; ++i) {
    rc_cell.WriteTrace(indices[i], values[i], SpanAdapter(trace));
  }

  rc_cell.Finalize(minimum, maximum, SpanAdapter(trace));

  // Make sure all holes are filled.
  std::vector<bool> value_set(maximum - minimum + 1);
  auto data = std::move(rc_cell).Consume();
  for (size_t i = 0; i < values_length; ++i) {
    ASSERT_EQ(data[indices[i]], values[i]);
  }
  for (auto value : data) {
    ASSERT_LE(value, maximum);
    ASSERT_LE(minimum, value);
    value_set[value - minimum] = true;
  }
  for (bool exists : value_set) {
    ASSERT_EQ(exists, true);
  }
}

TEST_F(RangeCheckCellTest, AllInitialized) {
  RangeCheckCell<FieldElementT> rc_cell("test", ctx, trace_length);
  std::vector<std::vector<FieldElementT>> trace = {
      std::vector<FieldElementT>(trace_length, FieldElementT::Zero())};
  for (size_t i = 0; i < trace_length; i++) {
    rc_cell.WriteTrace(i, i, SpanAdapter(trace));
  }

  EXPECT_ASSERT(
      rc_cell.Finalize(0, trace_length, SpanAdapter(trace)),
      HasSubstr(" Trace size is not large enough for range-check values. Range size: 257. Filled "
                "Holes: 0. Remaining holes: 1."));

  // Check that with rc_max = trace_length - 1 there is no error.
  rc_cell.Finalize(0, trace_length - 1, SpanAdapter(trace));
}

TEST_F(RangeCheckCellTest, InvalidRanges) {
  RangeCheckCell<FieldElementT> rc_cell("test", ctx, trace_length);
  std::vector<std::vector<FieldElementT>> trace = {
      std::vector<FieldElementT>(trace_length, FieldElementT::Zero())};
  EXPECT_ASSERT(
      rc_cell.Finalize(15, 10, SpanAdapter(trace)),
      HasSubstr("rc_min must be smaller than rc_max"));
  EXPECT_ASSERT(
      rc_cell.Finalize(15, (uint64_t)(-1), SpanAdapter(trace)),
      HasSubstr("rc_max must be smaller than"));
}

}  // namespace
}  // namespace starkware
