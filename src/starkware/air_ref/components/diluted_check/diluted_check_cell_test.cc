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

#include "starkware/air/components/diluted_check/diluted_check_cell.h"

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/components/trace_generation_context.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace diluted_check_cell {
namespace {

using testing::HasSubstr;

using FieldElementT = TestFieldElement;

class DilutedCheckCellTest : public ::testing::Test {
 public:
  const uint64_t trace_length = (Pow2(17));
  const uint64_t values_length = 60;
  const uint64_t spacing = 3;
  const uint64_t n_bits = 16;
  TraceGenerationContext ctx;

  DilutedCheckCellTest() {
    ctx.AddVirtualColumn("test", VirtualColumn(/*column=*/0, /*step=*/1, /*row_offset=*/0));
  }
};

/*
  Tests that Finalize() fills holes correctly.
*/
TEST_F(DilutedCheckCellTest, WriteTraceTest) {
  DilutedCheckCell<FieldElementT> diluted_cell("test", ctx, trace_length, spacing, n_bits);
  Prng prng;
  const auto indices = prng.UniformDistinctIntVector<uint64_t>(0, trace_length - 1, values_length);
  const auto values = prng.UniformIntVector<uint64_t>(0, (Pow2(n_bits)) - 1, values_length);

  // Initialize a single column trace.
  std::vector<std::vector<FieldElementT>> trace = {
      std::vector<FieldElementT>(trace_length, FieldElementT::Zero())};
  for (size_t i = 0; i < values_length; ++i) {
    diluted_cell.WriteTrace(indices[i], Dilute(values[i], spacing, n_bits), SpanAdapter(trace));
  }

  diluted_cell.Finalize(SpanAdapter(trace));

  // Make sure all holes are filled.
  std::vector<bool> value_set(Pow2(n_bits));
  auto data = std::move(diluted_cell).Consume();
  for (size_t i = 0; i < values_length; ++i) {
    ASSERT_EQ(data[indices[i]], Dilute(values[i], spacing, n_bits));
  }
  for (auto value : data) {
    ASSERT_LT(value, Pow2(spacing * n_bits));
    ASSERT_LE(0, value);
    ASSERT_EQ(Dilute(Undilute(value, spacing, n_bits), spacing, n_bits), value);
    value_set[Undilute(value, spacing, n_bits)] = true;
  }
  for (bool exists : value_set) {
    ASSERT_TRUE(exists);
  }
}

TEST_F(DilutedCheckCellTest, AllInitialized) {
  DilutedCheckCell<FieldElementT> diluted_cell("test", ctx, trace_length, spacing, n_bits);
  std::vector<std::vector<FieldElementT>> trace = {
      std::vector<FieldElementT>(trace_length, FieldElementT::Zero())};
  for (size_t i = 0; i < trace_length * 3 / 4; ++i) {
    diluted_cell.WriteTrace(i, Dilute(i / 4, spacing, n_bits), SpanAdapter(trace));
  }

  EXPECT_ASSERT(
      diluted_cell.Finalize(SpanAdapter(trace)),
      HasSubstr("Trace size is not large enough for dilluted-check values. Filled "
                "missing values: 32768. Remaining missing values: 8192."));
}

TEST_F(DilutedCheckCellTest, InvalidValue) {
  DilutedCheckCell<FieldElementT> diluted_cell("test", ctx, trace_length, spacing, n_bits);
  std::vector<std::vector<FieldElementT>> trace = {
      std::vector<FieldElementT>(trace_length, FieldElementT::Zero())};
  // 2 is not a diluted value.
  diluted_cell.WriteTrace(0, 2, SpanAdapter(trace));

  EXPECT_ASSERT(diluted_cell.Finalize(SpanAdapter(trace)), HasSubstr("Invalid diluted value: 2"));
}

}  // namespace
}  // namespace diluted_check_cell
}  // namespace starkware
