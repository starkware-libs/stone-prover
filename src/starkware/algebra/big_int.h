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

#ifndef STARKWARE_ALGEBRA_BIG_INT_H_
#define STARKWARE_ALGEBRA_BIG_INT_H_

#include <cstddef>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/uint128.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

// Used for Tag Dispatching to allow constructing an uninitialized BigInt.
struct UninitializedTag {};

static constexpr Uint128 Umul128(uint64_t x, uint64_t y) {
  return static_cast<Uint128>(x) * static_cast<Uint128>(y);
}

template <size_t N>
class BigInt {
 public:
  static constexpr size_t kN = N;
  static constexpr size_t kDigits = N * std::numeric_limits<uint64_t>::digits;
  static constexpr size_t kNibbles = SafeDiv(kDigits, 4);

  BigInt() = default;

  // Uninitialized BigInt constructor.
  explicit BigInt(UninitializedTag /*tag*/) {}

  template <size_t K>
  constexpr BigInt(const BigInt<K>& v) noexcept;  // NOLINT implicit cast.
  constexpr explicit BigInt(const std::array<uint64_t, N>& v) noexcept : value_(v) {}
  constexpr explicit BigInt(uint64_t v) noexcept : value_(std::array<uint64_t, N>({v})) {}

  constexpr uint64_t AsUint() noexcept;

  /*
    Constructs a BigInt<N> from a BigInt<K>. If K > N, asserts that the number is small enough.
  */
  template <size_t K>
  static constexpr BigInt FromBigInt(const BigInt<K>& other);

  static constexpr BigInt One() { return BigInt(std::array<uint64_t, N>({1})); }
  static constexpr BigInt Zero() { return BigInt(std::array<uint64_t, N>({0})); }

  static BigInt RandomBigInt(PrngBase* prng);

  /*
    Returns pair of the form (result, overflow_occurred).
  */
  static constexpr std::pair<BigInt, bool> Add(const BigInt& a, const BigInt& b);
  constexpr BigInt operator+(const BigInt& other) const { return Add(*this, other).first; }
  constexpr BigInt& operator+=(const BigInt& other) { return *this = *this + other; }
  constexpr BigInt operator-(const BigInt& other) const { return Sub(*this, other).first; }
  constexpr BigInt operator-() const { return Zero() - *this; }

  /*
    Multiplies two BigInt<N> numbers, this and other. Returns the result as a BigInt<2*N>.
  */
  constexpr BigInt<2 * N> operator*(const BigInt& other) const;

  /*
    Multiplies two BigInt<N> numbers modulo a third.
  */
  static BigInt MulMod(const BigInt& a, const BigInt& b, const BigInt& modulus);

  /*
    Adds two BigInt<N> numbers modulo a third.
  */
  static BigInt AddMod(const BigInt& a, const BigInt& b, const BigInt& modulus);

  void operator^=(const BigInt& other);

  /*
    Return pair of the form (result, underflow_occurred).
  */
  static constexpr std::pair<BigInt, bool> Sub(const BigInt& a, const BigInt& b);

  constexpr bool operator<(const BigInt& b) const;

  constexpr bool operator>=(const BigInt& b) const { return !(*this < b); }

  bool operator>(const BigInt& b) const { return b < *this; }

  bool operator<=(const BigInt& b) const { return !(*this > b); }

  /*
    Returns the pair (q,r) such that a=qb+r, and r<b.
  */
  static std::pair<BigInt, BigInt> Div(BigInt a, const BigInt& b);

  std::pair<BigInt, BigInt> Div(const BigInt& other) const { return BigInt::Div(*this, other); }

  /*
    Finds the inverse of value mod modulus.
    The implementation is based on the extended GCD algorithm,
    which given x,y finds (a,b,d) s.t. ax+by = d.
    However, Since we are only intersted in the inverse we only keep track of a and d.
    The function can probably be improved if we switch to binary GCD.
  */
  static constexpr BigInt Inverse(const BigInt& value, const BigInt& modulus);

  constexpr bool IsEven() const;

  constexpr bool IsMsbSet() const;

  static BigInt FromBytes(gsl::span<const std::byte> bytes, bool use_big_endian = true);

  void ToBytes(gsl::span<std::byte> span_out, bool use_big_endian = true) const;

  static BigInt FromString(const std::string& s);

  /*
    Returns the representation of the number as a string of the form "0x...".
  */
  std::string ToString() const;

  std::vector<bool> ToBoolVector() const;

  /*
    Returns an array of nibble values (hex digits) representing the number.
    The returned array contains the lsb first and msb last, i.e. little-endian.
  */
  std::array<uint8_t, kNibbles> ToNibbleArray() const;

  /*
    Returns (x % target) assuming x is in the range [0, 2*target).

    The function assumes that target.NumLeadingZeros() > 0.

    Typically used after a Montgomery reduction which produces an output that satisfies
    the range requirement above.
  */
  template <bool IsConstexpr = false>
  static constexpr BigInt ReduceIfNeeded(const BigInt& x, const BigInt& target);

  /*
    Calculates x/(2^64) mod modulus.
    This is a cheaper operation than the full Montgomery reduction (x/(2^64N) mod modulus).

    This function is useful for efficiently obtaining a non redundant represantation for
    serialization.
  */
  static BigInt OneLimbMontgomeryReduction(
      const BigInt& x, const BigInt& modulus, uint64_t montgomery_mprime);

  /*
    Calculates x*y/2^256 mod modulus, assuming that montgomery_mprime is (-(modulus^-1)) mod 2^64.
    Assumes that y < modulus and modulus.NumLeadingZeros() > 0.
  */
  static constexpr BigInt MontMul(
      const BigInt& x, const BigInt& y, const BigInt& modulus, uint64_t montgomery_mprime);

  auto rbegin() const { return value_.rbegin(); }  // NOLINT
  auto rend() const { return value_.rend(); }      // NOLINT

  auto begin() const { return value_.begin(); }  // NOLINT
  auto end() const { return value_.end(); }      // NOLINT

  constexpr bool operator==(const BigInt& other) const;

  constexpr bool operator!=(const BigInt& other) const { return !(*this == other); }

  constexpr uint64_t& operator[](int i) { return constexpr_at(value_, i); }

  constexpr const uint64_t& operator[](int i) const { return gsl::at(value_, i); }

  /*
    Bitwise AND operation.
  */
  BigInt operator&(const BigInt& other) const;

  /*
    Bitwise XOR operation.
  */
  BigInt operator^(const BigInt& other) const;

  /*
    Bitwise OR operation.
  */
  BigInt operator|(const BigInt& other) const;

  /*
    Bitwise SHIFT-RIGHT operation.
  */
  BigInt& operator>>=(size_t shift);
  constexpr BigInt operator>>(size_t shift) const { return BigInt(*this) >>= shift; }

  /*
    Bitwise SHIFT-LEFT operation.
  */
  constexpr BigInt& operator<<=(size_t shift);

  static constexpr size_t LimbCount() { return N; }
  static constexpr size_t SizeInBytes() { return N * sizeof(uint64_t); }

  /*
    Returns the number of leading zero's.
  */
  constexpr size_t NumLeadingZeros() const;

  /*
    Returns floor(Log_2(n)), n must be > 0.
  */
  constexpr size_t Log2Floor() const;

  /*
    Returns ceil(Log_2(n)), n must be > 0.
  */
  constexpr size_t Log2Ceil() const;

  /*
    Returns true if and only if the number is a power of two.
  */
  constexpr bool IsPowerOfTwo() const;

  /*
    Returns a copy of the BigInt with a hint to the compiler to put the BigInt in registers.

    This function is used to improve the generated machine code.
    Typically used becasue the compiler doesn't generate a proper carry chain when
    one of the inputs is a compile time const.
  */
  BigInt GetWithRegisterHint() const;

 private:
  std::array<uint64_t, N> value_;

  /*
    Shift Right entire words.
  */
  void RightShiftWords(size_t shift);

  /*
    Shift Left entire words.
  */
  void LeftShiftWords(size_t shift);
};

template <size_t N>
std::ostream& operator<<(std::ostream& os, const BigInt<N>& bigint);

}  // namespace starkware

/*
  Implements the user defined _Z literal that constructs a BigInt of an arbitrary size.
  For example:
    BigInt<4> a = 0x73eda753299d7d483339d80809a1d80553bda402fffe5bfeffffffff00000001_Z;
*/
template <char... Chars>
static constexpr auto operator"" _Z();

#include "starkware/algebra/big_int.inl"

#endif  // STARKWARE_ALGEBRA_BIG_INT_H_
