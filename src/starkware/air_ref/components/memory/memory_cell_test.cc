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

#include "starkware/air/components/memory/memory_cell.h"

#include <algorithm>
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

class MemoryCellTest : public ::testing::Test {
 public:
  const uint64_t address_col = 0;
  const uint64_t value_col = 1;
  TraceGenerationContext ctx;

  MemoryCellTest() {
    ctx.AddVirtualColumn(
        "test/addr", VirtualColumn(/*column=*/address_col, /*step=*/1, /*row_offset=*/0));
    ctx.AddVirtualColumn(
        "test/value", VirtualColumn(/*column=*/value_col, /*step=*/1, /*row_offset=*/0));
  }

  /*
    Tests Finalize function. Data will be generated filling used_slots_num slots, with addresses
    in [address_lower_bd, address_upper_bd]. If exact_range is true, than both boundary addresses
    will assuredly be used. The memory's size is then set to assure that the number of empty slots
    will suffice to fill all holes with spare_slots_num to spare: if spare_slots_num is negative,
    then there will be too few slots to fill the address gaps.
  */
  void FinalizeTestFunc(
      size_t used_slots_num, int64_t spare_slots_num, uint64_t address_lower_bd,
      uint64_t address_upper_bd, bool exact_range);
};

/*
  Tests WriteTrace function in two scenarios: when writing an address-value pair which is part of
  the public memory and when writing an address-value pair which is not part of the public memory.
  Verifies that the correct values are written both to the trace and to the MemoryCell's address_
  and value_ members.
*/
TEST_F(MemoryCellTest, WriteTraceTest) {
  const uint64_t trace_length = 256;
  const uint64_t memory_length = 128;
  MemoryCell<FieldElementT> memory_cell("test", ctx, trace_length);
  Prng prng;
  const auto indices = prng.UniformDistinctIntVector<uint64_t>(0, trace_length - 1, 2);
  const uint64_t idx1 = indices[0];
  const uint64_t address1 = prng.UniformInt<uint64_t>(0, memory_length - 1);
  const auto value1 = FieldElementT::RandomElement(&prng);

  // Initialize a two-column trace with ones.
  std::vector<std::vector<FieldElementT>> trace = {
      std::vector<FieldElementT>(trace_length, FieldElementT::One()),
      std::vector<FieldElementT>(trace_length, FieldElementT::One())};

  // Call WriteTrace with is_public_memory = false.
  memory_cell.WriteTrace(idx1, address1, value1, SpanAdapter(trace), false);
  EXPECT_EQ(trace[address_col][idx1], FieldElementT::FromUint(address1));
  EXPECT_EQ(trace[value_col][idx1], value1);

  // Call WriteTrace again with exactly the same values and verify that it fails.
  EXPECT_ASSERT(
      memory_cell.WriteTrace(idx1, address1, value1, SpanAdapter(trace), false),
      HasSubstr(std::to_string(idx1) + " was already written."));

  const uint64_t idx2 = indices[1];
  const uint64_t address2 = prng.UniformInt<uint64_t>(0, memory_length - 1);
  const auto value2 = FieldElementT::RandomElement(&prng);

  // Call WriteTrace with is_public_memory = true.
  memory_cell.WriteTrace(idx2, address2, value2, SpanAdapter(trace), true);
  EXPECT_EQ(trace[address_col][idx2], FieldElementT::Zero());
  EXPECT_EQ(trace[value_col][idx2], FieldElementT::Zero());

  // Call WriteTrace again with exactly the same values and verifies it fails.
  EXPECT_ASSERT(
      memory_cell.WriteTrace(idx2, address2, value2, SpanAdapter(trace), true),
      HasSubstr(std::to_string(idx2) + " was already written."));

  // Make sure the values of memory_cell.address_ and memory_cell.value_ are as expected
  // (address-value should be the original values that were set, and not zeros, even for public
  // memory values).
  // NOLINTNEXTLINE (auto [...]).
  const auto [address_vec, value_vec, public_input_indices] = std::move(memory_cell).Consume();
  // Avoid unused variable error.
  (void)public_input_indices;
  EXPECT_EQ(address_vec[indices[0]], address1);
  EXPECT_EQ(address_vec[indices[1]], address2);
  EXPECT_EQ(value_vec[indices[0]], value1);
  EXPECT_EQ(value_vec[indices[1]], value2);
}

void MemoryCellTest::FinalizeTestFunc(
    size_t used_slots_num, int64_t spare_slots_num, uint64_t address_lower_bd,
    uint64_t address_upper_bd, bool exact_range) {
  Prng prng;
  const auto dummy_value = FieldElementT::RandomElement(&prng);
  std::vector<uint64_t> addresses;

  // [address_min, address_max] will be the range occupied by the addresses. Initialize them as the
  // opposite bounds, so that after the first address is assigned, they will be equal to it.
  ASSERT_LE(address_lower_bd, address_upper_bd);
  uint64_t address_min = address_upper_bd;
  uint64_t address_max = address_lower_bd;

  if (exact_range) {
    ASSERT_GE(used_slots_num, 2);
    addresses.push_back(address_lower_bd);
    addresses.push_back(address_upper_bd);
    address_min = address_lower_bd;
    address_max = address_upper_bd;
  }
  for (size_t i = addresses.size(); i < used_slots_num; ++i) {
    uint64_t address = prng.UniformInt<uint64_t>(address_lower_bd, address_upper_bd);
    addresses.push_back(address);
    address_min = std::min(address_min, address);
    address_max = std::max(address_max, address);
  }

  std::vector<bool> orig_address_set(address_max - address_min + 1);
  for (auto address : addresses) {
    orig_address_set.at(address - address_min) = true;
  }
  const auto unused_addresses_num =
      std::count(orig_address_set.begin(), orig_address_set.end(), false);
  int64_t extra_slots = unused_addresses_num + spare_slots_num;
  const size_t data_length = std::max(used_slots_num, used_slots_num + extra_slots);
  ASSERT_GE(data_length, used_slots_num);

  MemoryCell<FieldElementT> memory_cell("test", ctx, data_length);
  // Initialize a two-column trace with ones.
  std::vector<std::vector<FieldElementT>> trace = {
      std::vector<FieldElementT>(data_length, FieldElementT::One()),
      std::vector<FieldElementT>(data_length, FieldElementT::One())};

  // Uniform sampling without repetition.
  std::vector<uint64_t> indices;
  for (size_t i = 0; i < data_length; ++i) {
    indices.push_back(i);
  }
  std::shuffle(
      indices.begin(), indices.end(),
      std::default_random_engine(prng.UniformInt<uint64_t>(0, 0xffffffffffffffffL)));
  indices.resize(used_slots_num);

  // For each i in [0, data_length), index_set[i] will be true if the slot i is filled by
  // WriteTrace, or false if it is filled by Finalize.
  std::vector<bool> index_set(data_length);

  for (size_t i = 0; i < used_slots_num; ++i) {
    bool is_public = static_cast<bool>(prng.UniformInt<uint64_t>(0, 1));
    memory_cell.WriteTrace(indices[i], addresses[i], dummy_value, SpanAdapter(trace), is_public);
    index_set.at(indices[i]) = true;
  }

  // Call finalize. Note that when spare_slots_num is negative, an assertion will be thrown here.
  memory_cell.Finalize(SpanAdapter(trace));

  // NOLINTNEXTLINE (auto [...]).
  const auto [address_vec, value_vec, public_input_indices] = std::move(memory_cell).Consume();
  // Avoid unused variable error.
  (void)public_input_indices;
  ASSERT_EQ(address_vec.size(), data_length);
  ASSERT_EQ(value_vec.size(), data_length);
  std::vector<bool> final_address_set(address_max - address_min + 2);

  for (size_t i = 0; i < used_slots_num; ++i) {
    ASSERT_EQ(addresses[i], address_vec[indices[i]]);
  }

  for (size_t i = 0; i < data_length; ++i) {
    // Check that the value at i was either set to dummy_value by WriteTrace, or filled with 0 by
    // Finalize.
    if (index_set[i]) {
      ASSERT_EQ(value_vec[i], dummy_value);
    } else {
      ASSERT_EQ(value_vec[i], FieldElementT::Zero());
    }
    const uint64_t address = address_vec[i];
    ASSERT_LE(address, address_max + 1);
    ASSERT_LE(address_min, address);
    final_address_set[address - address_min] = true;
  }
  // Check that all addresses in [address_min, address_max] were used.
  for (uint64_t address = address_min; address <= address_max; ++address) {
    ASSERT_EQ(final_address_set[address - address_min], true);
  }
  // Check that address_max + 1 was used exactly spare_slots_num times.
  ASSERT_EQ(spare_slots_num, std::count(address_vec.begin(), address_vec.end(), address_max + 1));
}

/*
  Tests Finalize function in several randomized settings with enough space to fill all gaps,
  some times with spare and some times without.
*/
TEST_F(MemoryCellTest, FinalizeRandomParamTests) {
  Prng prng;
  size_t used_slots_num = prng.UniformInt<size_t>(2, 32);
  int64_t spare_slots_num = prng.UniformInt<int64_t>(1, 96);
  uint64_t address_lower_bd = prng.UniformInt<size_t>(0, Pow2(14));
  uint64_t address_upper_bd = address_lower_bd + prng.UniformInt(8, 64);

  FinalizeTestFunc(used_slots_num, spare_slots_num, address_lower_bd, address_upper_bd, false);
  FinalizeTestFunc(used_slots_num, spare_slots_num, address_lower_bd, address_upper_bd, true);
  FinalizeTestFunc(1, spare_slots_num, address_lower_bd, address_upper_bd, false);
  FinalizeTestFunc(used_slots_num, 0, address_lower_bd, address_upper_bd, false);
}

/*
  Tests Finalize function in several predetermined settings with enough space to fill all gaps,
  sometimes with spare. Sometimes the data will completely fill the memory.
*/
TEST_F(MemoryCellTest, FinalizeFixedParamTests) {
  FinalizeTestFunc(4, 0, 7, 7, false);
  FinalizeTestFunc(4, 5, 7, 7, false);
  FinalizeTestFunc(6, 0, 7, 8, true);
  FinalizeTestFunc(6, 5, 7, 8, true);
  FinalizeTestFunc(30, 0, 17, 257, true);
  FinalizeTestFunc(30, 1, 17, 257, true);
  FinalizeTestFunc(30, 30, 17, 257, false);
}

/*
  Tests Finalize function in several settings where not enough empty slots are available for filling
  holes.
*/
TEST_F(MemoryCellTest, FinalizeNegativeSparesTests) {
  EXPECT_ASSERT(FinalizeTestFunc(30, -1, 17, 257, true), HasSubstr("Remaining holes: 1."));
  EXPECT_ASSERT(FinalizeTestFunc(2, -5, 17, 257, true), HasSubstr("Remaining holes: 5."));

  Prng prng;
  size_t used_slots_num = prng.UniformInt<size_t>(2, 32);
  int64_t spare_slots_num = -prng.UniformInt<int64_t>(1, 32);
  uint64_t address_lower_bd = prng.UniformInt<size_t>(0, 1 << 14);
  uint64_t address_upper_bd = address_lower_bd + prng.UniformInt(64, 128);
  EXPECT_ASSERT(
      FinalizeTestFunc(used_slots_num, spare_slots_num, address_lower_bd, address_upper_bd, true),
      HasSubstr("Remaining holes: " + std::to_string(-spare_slots_num)));
}

/*
  Test Finalize when all memory slots are used, and a memory gap exists: An assertion should fail.
*/
TEST_F(MemoryCellTest, AllInitialized) {
  const uint64_t trace_length = 256;
  MemoryCell<FieldElementT> memory_cell("test", ctx, trace_length);
  Prng prng;
  const auto dummy_value = FieldElementT::RandomElement(&prng);

  // Initialize a two-column trace with ones.
  std::vector<std::vector<FieldElementT>> trace = {
      std::vector<FieldElementT>(trace_length, FieldElementT::One()),
      std::vector<FieldElementT>(trace_length, FieldElementT::One())};

  memory_cell.WriteTrace(0, 0, dummy_value, SpanAdapter(trace), false);
  for (size_t i = 1; i < trace_length; ++i) {
    bool is_public = static_cast<bool>(prng.UniformInt<uint64_t>(0, 1));
    memory_cell.WriteTrace(i, i + 1, dummy_value, SpanAdapter(trace), is_public);
  }

  EXPECT_ASSERT(
      memory_cell.Finalize(SpanAdapter(trace)),
      HasSubstr("Available memory size was not large enough to fill holes in memory address range. "
                "Memory address range: 257. Filled holes: 0. Remaining holes: 1."));
}

}  // namespace
}  // namespace starkware
