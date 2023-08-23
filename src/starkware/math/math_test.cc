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

#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(Math, Pow2) { EXPECT_EQ(32U, Pow2(5)); }
TEST(Math, Pow2_0) { EXPECT_EQ(1U, Pow2(0)); }
TEST(Math, Pow2_63) { EXPECT_EQ(UINT64_C(0x8000000000000000), Pow2(63)); }
TEST(Math, Pow2_Constexpr) {
  static_assert(
      Pow2(0) == 1U, "The Pow2 function doesn't perform as expected in compilation time.");
}

TEST(Math, IsPowerOfTwo_32) { EXPECT_TRUE(IsPowerOfTwo(32)); }
TEST(Math, IsPowerOfTwo_31) { EXPECT_FALSE(IsPowerOfTwo(31)); }
TEST(Math, IsPowerOfTwo_33) { EXPECT_FALSE(IsPowerOfTwo(33)); }
TEST(Math, IsPowerOfTwo_zero) { EXPECT_FALSE(IsPowerOfTwo(0)); }
TEST(Math, IsPowerOfTwo_one) { EXPECT_TRUE(IsPowerOfTwo(1)); }
TEST(Math, IsPowerOfTwo_Constexpr) {
  static_assert(
      IsPowerOfTwo(1),
      "The IsPowerOfTwo function doesn't perform as expected in compilation time.");
}

TEST(Math, Log2Floor_0) { EXPECT_ASSERT(Log2Floor(0), HasSubstr("log2 of 0 is undefined")); }
TEST(Math, Log2Floor_1) { EXPECT_EQ(Log2Floor(1), 0U); }
TEST(Math, Log2Floor_31) { EXPECT_EQ(Log2Floor(31), 4U); }
TEST(Math, Log2Floor_32) { EXPECT_EQ(Log2Floor(32), 5U); }
TEST(Math, Log2Floor_33) { EXPECT_EQ(Log2Floor(33), 5U); }
TEST(Math, Log2Floor_AllBitsSet) { EXPECT_EQ(Log2Floor(~UINT64_C(0)), 63U); }
TEST(Math, Log2Floor_Constexpr) {
  static_assert(
      Log2Floor(1) == 0U,
      "The Log2Floor function doesn't perform as expected in compilation time.");
}

TEST(Math, SafeLog2_0) { EXPECT_ASSERT(SafeLog2(0), HasSubstr("must be a power of 2")); }
TEST(Math, SafeLog2_1) { EXPECT_EQ(SafeLog2(1), 0U); }
TEST(Math, SafeLog2_31) { EXPECT_ASSERT(SafeLog2(31), HasSubstr("must be a power of 2")); }
TEST(Math, SafeLog2_32) { EXPECT_EQ(SafeLog2(32), 5U); }
TEST(Math, SafeLog2_33) { EXPECT_ASSERT(SafeLog2(33), HasSubstr("must be a power of 2")); }
TEST(Math, SafeLog2_Constexpr) {
  static_assert(
      SafeLog2(1) == 0U, "The SafeLog2 function doesn't perform as expected in compilation time.");
}

TEST(Math, DivRoundUp_7_3) { EXPECT_EQ(DivCeil(7, 3), 3U); }
TEST(Math, DivRoundUp_16_4) { EXPECT_EQ(DivCeil(16, 4), 4U); }
TEST(Math, DivRoundUp_17_4) { EXPECT_EQ(DivCeil(17, 4), 5U); }
TEST(Math, DivRoundUp_Constexpr) {
  static_assert(
      DivCeil(1, 1) == 1U, "The DivCeil function doesn't perform as expected in compilation time.");
}

TEST(Math, SafeDiv_8_4) { EXPECT_EQ(SafeDiv(8, 4), 2U); }
TEST(Math, SafeDiv_0_0) { EXPECT_ASSERT(SafeDiv(0, 0), HasSubstr("Denominator cannot be zero")); }
TEST(Math, SafeDiv_8_0) { EXPECT_ASSERT(SafeDiv(8, 0), HasSubstr("Denominator cannot be zero")); }
TEST(Math, SafeDiv_17_7) {
  EXPECT_ASSERT(
      SafeDiv(17, 7),
      HasSubstr("The denominator 7 doesn't divide the numerator 17 without remainder"));
}
TEST(Math, SafeDiv_4_8) {
  EXPECT_ASSERT(
      SafeDiv(4, 8),
      HasSubstr("The denominator 8 doesn't divide the numerator 4 without remainder"));
}
TEST(Math, SafeDiv_Constexpr) {
  static_assert(
      SafeDiv(8, 4) == 2U, "The SafeDiv function doesn't perform as expected in compilation time.");
}

TEST(Math, SafeSub_0_0) { EXPECT_EQ(SafeSub(0, 0), 0U); }
TEST(Math, SafeSub_32_5) { EXPECT_EQ(SafeSub(32, 5), 27U); }
TEST(Math, SafeSub_AllBitsSet_AllBitsSet) { EXPECT_EQ(SafeSub(~UINT64_C(0), ~UINT64_C(0)), 0U); }
TEST(Math, SafeSub_0_1) {
  EXPECT_ASSERT(
      SafeSub(0, 1), HasSubstr("The subtrahend 1 must not be greater than the minuend 0"));
}
TEST(Math, SafeSub_5_32) {
  EXPECT_ASSERT(
      SafeSub(5, 32), HasSubstr("The subtrahend 32 must not be greater than the minuend 5"));
}
TEST(Math, SafeSub_AllBitsSet_0) { EXPECT_EQ(SafeSub(~UINT64_C(0), 0), ~UINT64_C(0)); }
TEST(Math, SafeSub_Constexpr) {
  static_assert(
      SafeSub(8, 4) == 4U, "The SafeSub function doesn't perform as expected in compilation time.");
}

TEST(Math, SafeSignedAdd_0_0) { EXPECT_EQ(SafeSignedAdd(0, 0), 0); }
TEST(Math, SafeSignedAdd_32_5) { EXPECT_EQ(SafeSignedAdd(32, 5), 37); }
TEST(Math, SafeSignedAdd_MaxPositive) {
  EXPECT_EQ(SafeSignedAdd(0, INT64_MAX), INT64_MAX);
  EXPECT_EQ(SafeSignedAdd(1, INT64_MAX - 1), INT64_MAX);
  EXPECT_ASSERT(
      SafeSignedAdd(1, INT64_MAX), HasSubstr("Got overflow/underflow in 1 + 9223372036854775807"));

  EXPECT_EQ(SafeSignedAdd(INT64_C(0x2403a2b090511cee), INT64_C(0x5bfc5d4f6faee311)), INT64_MAX);
  EXPECT_ASSERT(
      SafeSignedAdd(INT64_C(0x2403a2b090511cee), INT64_C(0x5bfc5d4f6faee312)),
      HasSubstr("Got overflow/underflow in 2595096689514716398 + 6628275347340059410"));
}
TEST(Math, SafeSignedAdd_MaxOverflow) {
  EXPECT_ASSERT(
      SafeSignedAdd(INT64_MAX, INT64_MAX),
      HasSubstr("Got overflow/underflow in 9223372036854775807 + 9223372036854775807"));
}
TEST(Math, SafeSignedAdd_MaxNegative) {
  EXPECT_EQ(SafeSignedAdd(0, INT64_MIN), INT64_MIN);
  EXPECT_EQ(SafeSignedAdd(-1, INT64_MIN + 1), INT64_MIN);
  EXPECT_ASSERT(
      SafeSignedAdd(-1, INT64_MIN),
      HasSubstr("Got overflow/underflow in -1 + -9223372036854775808"));

  EXPECT_EQ(SafeSignedAdd(-INT64_C(0x2403a2b090511cee), -INT64_C(0x5bfc5d4f6faee312)), INT64_MIN);
  EXPECT_ASSERT(
      SafeSignedAdd(-INT64_C(0x2403a2b090511cee), -INT64_C(0x5bfc5d4f6faee313)),
      HasSubstr("Got overflow/underflow in -2595096689514716398 + -6628275347340059411"));
}
TEST(Math, SafeSignedAdd_MaxUnderflow) {
  EXPECT_ASSERT(
      SafeSignedAdd(INT64_MIN, INT64_MIN),
      HasSubstr("Got overflow/underflow in -9223372036854775808 + -9223372036854775808"));
}

TEST(Math, SafeSignedNeg) {
  EXPECT_EQ(SafeSignedNeg(0), 0);
  EXPECT_EQ(SafeSignedNeg(0x2403a2b090511cee), -0x2403a2b090511cee);
  EXPECT_EQ(SafeSignedNeg(INT64_MAX), INT64_MIN + 1);
  EXPECT_EQ(SafeSignedNeg(INT64_MIN + 1), INT64_MAX);
  EXPECT_ASSERT(
      SafeSignedNeg(INT64_MIN), HasSubstr("Got overflow in SafeSignedNeg: -9223372036854775808"));
}

TEST(Math, Log2Ceil_1) { EXPECT_EQ(Log2Ceil(1), 0U); }
TEST(Math, Log2Ceil_31) { EXPECT_EQ(Log2Ceil(31), 5U); }
TEST(Math, Log2Ceil_32) { EXPECT_EQ(Log2Ceil(32), 5U); }
TEST(Math, Log2Ceil_33) { EXPECT_EQ(Log2Ceil(33), 6U); }
TEST(Math, Log2Ceil_AllBitsSet) { EXPECT_EQ(Log2Ceil(~UINT64_C(0)), 64U); }
TEST(Math, Log2Ceil_0) { EXPECT_ASSERT(Log2Ceil(0), HasSubstr("log2 of 0 is undefined")); }
TEST(Math, Log2Ceil_Constexpr) {
  static_assert(
      Log2Ceil(1) == 0U, "The Log2Ceil function doesn't perform as expected in compilation time.");
}

TEST(Math, Modulo) {
  EXPECT_EQ(Modulo(-4, 3), 2U);
  EXPECT_EQ(Modulo(-3, 3), 0U);
  EXPECT_EQ(Modulo(-2, 3), 1U);
  EXPECT_EQ(Modulo(-1, 3), 2U);
  EXPECT_EQ(Modulo(0, 3), 0U);
  EXPECT_EQ(Modulo(1, 3), 1U);
  EXPECT_EQ(Modulo(2, 3), 2U);
  EXPECT_EQ(Modulo(3, 3), 0U);
  EXPECT_EQ(Modulo(4, 3), 1U);
  EXPECT_EQ(Modulo(-298 * 345 + 17, 345), 17U);
}

}  // namespace
}  // namespace starkware
