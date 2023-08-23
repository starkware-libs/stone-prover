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

#ifndef STARKWARE_MATH_MATH_H_
#define STARKWARE_MATH_MATH_H_

#include <cstdint>
#include <vector>

#include "starkware/error_handling/error_handling.h"

namespace starkware {

using std::size_t;

constexpr uint64_t inline Pow2(uint64_t n) {
  ASSERT_RELEASE(n < 64, "n must be smaller than 64.");
  return UINT64_C(1) << n;
}

constexpr bool inline IsPowerOfTwo(const uint64_t n) { return (n != 0) && ((n & (n - 1)) == 0); }

/*
  Returns floor(Log_2(n)), n must be > 0.
*/
constexpr size_t inline Log2Floor(const uint64_t n) {
  ASSERT_RELEASE(n != 0, "log2 of 0 is undefined");
  static_assert(sizeof(long long) == 8, "It is assumed that long long is 64bits");  // NOLINT
  return 63 - __builtin_clzll(n);
}

/*
  Returns ceil(Log_2(n)), n must be > 0.
*/
constexpr size_t inline Log2Ceil(const uint64_t n) {
  ASSERT_RELEASE(n != 0, "log2 of 0 is undefined");
  return Log2Floor(n) + (IsPowerOfTwo(n) ? 0 : 1);
}

/*
  Computes log2(n) where n is a power of 2. This function fails if n is not a power of 2.
*/
constexpr size_t inline SafeLog2(const uint64_t n) {
  ASSERT_RELEASE(IsPowerOfTwo(n), "n must be a power of 2. n=" + std::to_string(n));
  return Log2Floor(n);
}

/*
  Computes x / y . This function fails if x % y != 0.
*/
constexpr uint64_t SafeDiv(const uint64_t numerator, const uint64_t denominator) {
  ASSERT_RELEASE(denominator != 0, "Denominator cannot be zero");
  ASSERT_RELEASE(
      numerator % denominator == 0, "The denominator " + std::to_string(denominator) +
                                        " doesn't divide the numerator " +
                                        std::to_string(numerator) + " without remainder");
  return numerator / denominator;
}

/*
  Computes x - y. This function fails if x < y.
*/
constexpr uint64_t SafeSub(const uint64_t minuend, const uint64_t subtrahend) {
  ASSERT_RELEASE(
      minuend >= subtrahend, "The subtrahend " + std::to_string(subtrahend) +
                                 " must not be greater than the minuend " +
                                 std::to_string(minuend));
  return minuend - subtrahend;
}

/*
  Computes x + y. This function fails if the result overflows or underflows.
*/
inline int64_t SafeSignedAdd(const int64_t a, const int64_t b) {
  int64_t res;
  ASSERT_RELEASE(
      !__builtin_saddl_overflow(a, b, &res),
      "Got overflow/underflow in " + std::to_string(a) + " + " + std::to_string(b));
  return res;
}

/*
  Computes -x. This function fails if the result overflows (this happens if x = -2**63).
*/
inline int64_t SafeSignedNeg(const int64_t x) {
  ASSERT_RELEASE(x != INT64_MIN, "Got overflow in SafeSignedNeg: " + std::to_string(x));
  return -x;
}

/*
  Computes ceil(x / y).
*/
constexpr uint64_t DivCeil(const uint64_t numerator, const uint64_t denominator) {
  return (numerator + denominator - 1) / denominator;
}

/*
  Returns a value 0 <= y < N congruent to x modulo n.
*/
uint64_t inline Modulo(const int64_t x, const uint64_t n) {
  ASSERT_DEBUG(n > 1, "modulus can not be zero nor one");
  return (x >= 0) ? (x % n) : n - 1 - ((-(x + 1)) % n);
}

/*
  Computes base to the power of the number given by exponent_bits in a generic group, given the
  element one in the group and a function mult(const GroupElementT& multiplier, GroupElementT* dst)
  that performs:
    *dst *= multiplier
  in the group.
  Note that it is possible that the address of multiplier is the same as dst.
*/
template <typename GroupElementT, typename MultFunc>
GroupElementT GenericPow(
    const GroupElementT& base, const std::vector<bool>& exponent_bits, const GroupElementT& one,
    const MultFunc& mult) {
  GroupElementT power = base;
  GroupElementT res = one;
  for (const auto&& b : exponent_bits) {
    if (b) {
      mult(power, &res);
    }

    mult(power, &power);
  }

  return res;
}

}  // namespace starkware

#endif  // STARKWARE_MATH_MATH_H_
