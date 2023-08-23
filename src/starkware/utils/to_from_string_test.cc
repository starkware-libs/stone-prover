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

#include "starkware/utils/to_from_string.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::HasSubstr;

inline std::string ListToString(const std::vector<uint8_t>& data) {
  return BytesToHexString(gsl::make_span(data).as_span<const std::byte>());
}

TEST(BytesToHexString, Basic) {
  EXPECT_ASSERT(ListToString({}), HasSubstr("Cannot convert"));
  EXPECT_EQ("0x0", ListToString({0}));
  EXPECT_EQ("0xa", ListToString({10}));
  EXPECT_EQ("0xaff", ListToString({10, 255}));
  EXPECT_EQ("0xf09bc", ListToString({15, 9, 188}));
}

TEST(BytesToHexString, IgnoreLeadingZeros) {
  Prng prng;
  for (size_t i = 0; i < 100; ++i) {
    size_t length = prng.UniformInt(1, 20);
    size_t zeros_prefix_length = prng.UniformInt(0UL, length - 1);
    std::vector<std::byte> with_leading(length, std::byte(0));
    std::vector<std::byte> no_leading(length - zeros_prefix_length);
    for (size_t j = 0; j < no_leading.size(); ++j) {
      no_leading[j] = std::byte(prng.UniformInt(0, 255));
      with_leading[zeros_prefix_length + j] = no_leading[j];
    }
    ASSERT_EQ(BytesToHexString(with_leading), BytesToHexString(no_leading));
  }
}

TEST(BytesToHexString, DontIgnoreLeadingZeros) {
  Prng prng;
  for (size_t i = 0; i < 100; ++i) {
    std::stringstream test_str;
    test_str << "0x";
    size_t length = prng.UniformInt(1, 20);
    size_t zero_bytes_prefix_length = prng.UniformInt(0UL, length - 1);
    std::vector<std::byte> with_leading(length, std::byte(0));
    size_t j = 0;
    for (; j < zero_bytes_prefix_length; ++j) {
      // Each zero byte translates into two zero nibbles.
      test_str << "00";
    }

    for (; j < length; ++j) {
      with_leading[j] = std::byte(prng.UniformInt(0, 255));
      test_str << std::hex << std::setfill('0') << std::setw(2)
               << std::to_integer<int>(with_leading[j]);
    }
    ASSERT_EQ(BytesToHexString(with_leading, false), test_str.str());
  }
}

TEST(HexStringToBytes, Basic) {
  // Empty string.
  std::vector<std::byte> byte_rep;
  EXPECT_ASSERT(HexStringToBytes("", byte_rep), HasSubstr("too short"));
  // Several bytes.
  std::vector<std::vector<uint8_t>> values = {{0x1a}, {0x1a, 0x0f, 0xff}, {0x11},
                                              {0x9},  {0x55, 0x55},       {0x12, 0x34, 0x56, 0x78}};
  std::vector<std::string> hex_reps = {"0x1a", "0x1a0fff", "0x11", "0x09", "0x5555", "0x12345678"};
  for (unsigned i = 0; i < values.size(); ++i) {
    gsl::span<const std::byte> as_bytes = gsl::make_span(values[i]).as_span<std::byte>();
    byte_rep.resize(hex_reps[i].length() / 2 - 1);
    HexStringToBytes(hex_reps[i], byte_rep);
    EXPECT_EQ(gsl::make_span(byte_rep), as_bytes);
  }
}

TEST(HexStringToBytes, ToStringAndBack) {
  Prng prng;
  for (size_t i = 0; i < 1000; ++i) {
    size_t length = prng.UniformInt(1, 100);
    std::vector<std::byte> byte_form1(length), byte_form2(length);
    prng.GetRandomBytes(byte_form1);
    HexStringToBytes(BytesToHexString(byte_form1), byte_form2);
    ASSERT_EQ(byte_form1, byte_form2);
  }
}

TEST(HexStringToBytes, ToStringAndBackNoTrim) {
  Prng prng;
  for (size_t i = 0; i < 1000; ++i) {
    size_t length = prng.UniformInt(1, 100);
    std::vector<std::byte> byte_form1(length), byte_form2(length);
    prng.GetRandomBytes(byte_form1);
    HexStringToBytes(BytesToHexString(byte_form1, false), byte_form2);
    ASSERT_EQ(byte_form1, byte_form2);
  }
}

TEST(StrToUint64, General) {
  EXPECT_EQ(StrToUint64("0"), 0);
  EXPECT_EQ(StrToUint64("1"), 1);
  EXPECT_EQ(StrToUint64(std::to_string(uint64_t(1) << 63)), uint64_t(1) << 63);

  Prng prng;
  const uint64_t rand = prng.UniformInt<uint64_t>(0, std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(StrToUint64(std::to_string(rand)), rand);

  EXPECT_ASSERT(StrToUint64(""), HasSubstr("does not represent"));
  EXPECT_ASSERT(StrToUint64("01"), HasSubstr("does not represent"));
  EXPECT_ASSERT(StrToUint64("-0"), HasSubstr("does not represent"));
  EXPECT_ASSERT(StrToUint64("-1"), HasSubstr("does not represent"));
  EXPECT_ASSERT(StrToUint64("f-1sf"), HasSubstr("does not represent"));

  // Test 2^64.
  EXPECT_ASSERT(StrToUint64("18446744073709551616"), HasSubstr("does not represent"));

  // Test 2^70.
  EXPECT_ASSERT(StrToUint64("1180591620717411303424"), HasSubstr("does not represent"));
}

}  // namespace
}  // namespace starkware
