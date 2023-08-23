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

#ifndef STARKWARE_ALGEBRA_FIELDS_LONG_FIELD_ELEMENT_H_
#define STARKWARE_ALGEBRA_FIELDS_LONG_FIELD_ELEMENT_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

#include "starkware/algebra/big_int.h"
#include "starkware/algebra/field_element_base.h"
#include "starkware/algebra/uint128.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/randomness/prng.h"

namespace starkware {

/*
  This is a simplified implementation of PrimeFieldElement for the case where
  N=1 -> kModulus fits in a single uint64
  LongFieldElements are stored in montgomery representation.
*/
class LongFieldElement : public FieldElementBase<LongFieldElement> {
 public:
  static constexpr uint64_t kModulus = 0x2000001400000001;  // 2**61 + 20 * 2**32 + 1.
  static constexpr uint64_t kModulusBits = Log2Floor(kModulus);
  static constexpr uint64_t kMontgomeryR = 0x1fffff73fffffff9;  // = 2^64 % kModulus.
  static constexpr uint64_t kMontgomeryRSquared = 0x1fc18a13fffce041;
  static constexpr uint64_t kMontgomeryRCubed = 0x1dcf974ec7cafec4;
  static constexpr uint64_t kMontgomeryMPrime = 0x20000013ffffffff;  // = (-(kModulus^-1)) mod 2^64.

#ifdef NDEBUG
  // We allow the use of the default constructor only in Release builds in order to reduce
  // memory allocation time for vectors of field elements.
  LongFieldElement() = default;
#else
  // In debug builds, we make sure that the default constructor is not called at all.
  LongFieldElement() = delete;
#endif

  static constexpr LongFieldElement Zero() { return LongFieldElement(0); }

  static constexpr LongFieldElement One() { return LongFieldElement(kMontgomeryR); }

  static LongFieldElement Uninitialized() {
    return Zero();
  }

  static constexpr LongFieldElement FromUint(uint64_t val) {
    // Note that because MontgomeryMul divides by r so we need to multiply by r^2 here.
    return LongFieldElement(MontgomeryMul(val, kMontgomeryRSquared));
  }

  LongFieldElement operator+(const LongFieldElement& rhs) const {
    return LongFieldElement(ReduceIfNeeded(value_ + rhs.value_, kModulus));
  }

  LongFieldElement operator-(const LongFieldElement& rhs) const {
    uint64_t val = value_ - rhs.value_;

    return LongFieldElement(IsNegative(val) ? val + kModulus : val);
  }

  LongFieldElement operator-() const { return Zero() - *this; }

  LongFieldElement operator*(const LongFieldElement& rhs) const {
    return LongFieldElement(MontgomeryMul(value_, rhs.value_));
  }

  constexpr bool operator==(const LongFieldElement& rhs) const { return value_ == rhs.value_; }

  constexpr LongFieldElement Inverse() const {
    ASSERT_RELEASE(*this != LongFieldElement::Zero(), "Zero does not have an inverse");
    return InverseToMontgomery(BigInt<1>::Inverse(BigInt<1>(value_), BigInt<1>(kModulus)));
  }

  // Returns a byte serialization of the field element.
  void ToBytes(gsl::span<std::byte> span_out, bool use_big_endian = true) const;

  static LongFieldElement RandomElement(PrngBase* prng);

  static LongFieldElement FromBytes(gsl::span<const std::byte> bytes, bool use_big_endian = true);

  static LongFieldElement FromString(const std::string& s);

  std::string ToString() const;

  BigInt<1> ToStandardForm() const;

  static constexpr BigInt<1> FieldSize() { return BigInt<1>(kModulus); }
  static constexpr LongFieldElement Generator() { return LongFieldElement::FromUint(3); }
  static constexpr std::array<BigInt<1>, 5> PrimeFactors() {
    return {BigInt<1>(2), BigInt<1>(13), BigInt<1>(167), BigInt<1>(211), BigInt<1>(293)};
  }
  static constexpr size_t SizeInBytes() { return sizeof(uint64_t); }
  static constexpr uint64_t Characteristic() { return kModulus; }

  static LongFieldElement FromMontgomeryForm(uint64_t val) { return LongFieldElement(val); }

  /*
    This is a variation on algorithm 4 from the "Faster Arithmetic for Number-Theoretic Transforms"
    paper.
    It assumes that kModulus is at most 62 bits long.
    twiddle_factor is in the range [0, kModulus)
    Inputs and outputs are in the range [0, 4*kModulus).
  */
  static void FftButterfly(
      const LongFieldElement& in1, const LongFieldElement& in2,
      const LongFieldElement& twiddle_factor, LongFieldElement* out1, LongFieldElement* out2) {
    static_assert(
        Log2Floor(LongFieldElement::kModulus) <= std::numeric_limits<uint64_t>::digits - 2,
        "Not enough redundency bits");

    const uint64_t mul_res = UnreducedMontgomeryMul(in2.value_, twiddle_factor.value_);
    const uint64_t tmp = ReduceIfNeeded(in1.value_, 2 * kModulus);

    // We write out2 first because out1 may alias in1.
    *out2 = LongFieldElement(tmp + 2 * kModulus - mul_res);
    *out1 = LongFieldElement(tmp + mul_res);
  }

  static void FftNormalize(LongFieldElement* val) {
    *val = LongFieldElement(ReduceIfNeeded(ReduceIfNeeded(val->value_, 2 * kModulus), kModulus));
  }

 private:
  explicit constexpr LongFieldElement(uint64_t val) : value_(val) {}

  static constexpr bool IsNegative(uint64_t val) { return static_cast<int64_t>(val) < 0; }

  static constexpr uint64_t ReduceIfNeeded(uint64_t val, uint64_t target) {
    uint64_t alt_val = val - target;
    return IsNegative(alt_val) ? val : alt_val;
  }

  static constexpr LongFieldElement InverseToMontgomery(BigInt<1> value) {
    return LongFieldElement(MontgomeryMul(value[0], kMontgomeryRCubed));
  }

  /*
    Computes (x*y / (2^64)) mod kModulus.
  */
  static constexpr uint64_t MontgomeryMulImpl(
      const uint64_t x, const uint64_t y, const uint64_t modulus, uint64_t montgomery_mprime) {
    Uint128 mul_res = Umul128(x, y);
    uint64_t u = static_cast<uint64_t>(mul_res) * montgomery_mprime;
    Uint128 res = Umul128(modulus, u) + mul_res;

    ASSERT_DEBUG(static_cast<uint64_t>(res) == 0, "Low 64bit should be 0");
    return static_cast<uint64_t>(res >> 64);
  }

  static constexpr uint64_t UnreducedMontgomeryMul(uint64_t x, uint64_t y) {
    return MontgomeryMulImpl(x, y, kModulus, kMontgomeryMPrime);
  }

  static constexpr uint64_t MontgomeryMul(uint64_t x, uint64_t y) {
    return ReduceIfNeeded(UnreducedMontgomeryMul(x, y), kModulus);
  }

  uint64_t value_ = 0;
};

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_FIELDS_LONG_FIELD_ELEMENT_H_
