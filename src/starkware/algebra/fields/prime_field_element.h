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

#ifndef STARKWARE_ALGEBRA_FIELDS_PRIME_FIELD_ELEMENT_H_
#define STARKWARE_ALGEBRA_FIELDS_PRIME_FIELD_ELEMENT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>

#include "starkware/algebra/big_int.h"
#include "starkware/algebra/field_element_base.h"
#include "starkware/algebra/fields/big_prime_constants.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/randomness/prng.h"

namespace starkware {

// Index specifies the large prime defining the field (from BigPrimeConstants struct).
// It is a key to an array of structs with relevant numbers (kModulus, kMontgomeryR, etc.).

// PrimeFieldElements are stored in montgomery representation.
template <int NBits, int Index>
class PrimeFieldElement : public FieldElementBase<PrimeFieldElement<NBits, Index>> {
 private:
  using kBigPrimeConstants = BigPrimeConstants<NBits, Index>;

 public:
  using ValueType = typename BigPrimeConstants<NBits, Index>::ValueType;

#ifdef NDEBUG
  // We allow the use of the default constructor only in Release builds in order to reduce
  // memory allocation time for vectors of field elements.
  PrimeFieldElement() : value_(UninitializedTag()) {}
#else
  // In debug builds, we make sure that the default constructor is not called at all.
  PrimeFieldElement() = delete;
#endif

  explicit PrimeFieldElement(UninitializedTag /*tag*/) : value_(UninitializedTag()) {}

  static PrimeFieldElement Uninitialized() { return PrimeFieldElement(UninitializedTag()); }

  static PrimeFieldElement FromUint(uint64_t val) {
    return PrimeFieldElement(
        // Note that because MontgomeryMul divides by r we need to multiply by r^2 here.
        MontgomeryMul(ValueType(val), kBigPrimeConstants::kMontgomeryRSquared));
  }

  static constexpr PrimeFieldElement FromBigInt(const ValueType& val) {
    return PrimeFieldElement(
        // Note that because MontgomeryMul divides by r we need to multiply by r^2 here.
        MontgomeryMul(val, kBigPrimeConstants::kMontgomeryRSquared));
  }

  static constexpr PrimeFieldElement ConstexprFromBigInt(const ValueType& val) {
    return PrimeFieldElement(
        // Note that because MontgomeryMul divides by r we need to multiply by r^2 here.
        MontgomeryMul<true>(val, kBigPrimeConstants::kMontgomeryRSquared));
  }

  static PrimeFieldElement FromMontgomeryForm(const ValueType& val) {
    return PrimeFieldElement(val);
  }

  static PrimeFieldElement RandomElement(PrngBase* prng);

  static PrimeFieldElement FromBytes(gsl::span<const std::byte> bytes, bool use_big_endian = true);

  static PrimeFieldElement FromString(const std::string& s);

  static constexpr size_t SizeInBytes() { return ValueType::SizeInBytes(); }

  static constexpr PrimeFieldElement Zero() { return PrimeFieldElement(ValueType({})); }

  static constexpr PrimeFieldElement One() {
    return PrimeFieldElement(kBigPrimeConstants::kMontgomeryR);
  }

  PrimeFieldElement operator*(const PrimeFieldElement& rhs) const {
    return PrimeFieldElement(MontgomeryMul(value_, rhs.value_));
  }

  PrimeFieldElement operator+(const PrimeFieldElement& rhs) const {
    return PrimeFieldElement{
        ValueType::ReduceIfNeeded(value_.GetWithRegisterHint() + rhs.value_, GetModulus())};
  }

  constexpr PrimeFieldElement operator-(const PrimeFieldElement& rhs) const {
    ValueType res = value_ - rhs.value_;
    if (res.IsMsbSet()) {
      res = res + GetModulus();
    }

    return PrimeFieldElement{res};
  }

  constexpr PrimeFieldElement operator-() const { return Zero() - *this; }

  bool operator==(const PrimeFieldElement& rhs) const { return value_ == rhs.value_; }

  PrimeFieldElement Inverse() const {
    ASSERT_RELEASE(*this != PrimeFieldElement::Zero(), "Zero does not have an inverse");
    return InverseToMontgomery(ValueType::Inverse(value_, kBigPrimeConstants::kModulus));
  }

  // Returns a byte serialization of the field element.
  void ToBytes(gsl::span<std::byte> span_out, bool use_big_endian = true) const;

  /*
    Returns the standard representation.

    A value in the range [0, kBigPrimeConstants::kModulus) in non-redundant non-Montogomery
    representation.
  */
  ValueType ToStandardForm() const { return MontgomeryMul(value_, ValueType::One()); }

  // Chosen for fft considerations.
  static constexpr ValueType GetModulus() { return kBigPrimeConstants::kModulus; }
  static constexpr ValueType FieldSize() { return GetModulus(); }
  static constexpr ValueType Characteristic() { return GetModulus(); }

  static constexpr PrimeFieldElement Generator() {
    return FromUint(kBigPrimeConstants::kGenerator);
  }
  static constexpr auto PrimeFactors() { return kBigPrimeConstants::kFactors; }
  // Maximum value for ValueType that is divisible by Modulus.
  static constexpr ValueType GetMaxDivisible() { return kBigPrimeConstants::kMaxDivisible; }

  std::string ToString() const { return ToStandardForm().ToString(); }

  /*
    This is a variation on algorithm 4 from the "Faster Arithmetic for Number-Theoretic Transforms"
    paper.
    It assumes that ValueType has enough bits to hold the value 4*kModulus - 1.
    twiddle_factor is in the range [0, kModulus)
    Inputs and outputs are in the range [0, 4*kModulus).
  */
  static void FftButterfly(
      const PrimeFieldElement& in1, const PrimeFieldElement& in2,
      const PrimeFieldElement& twiddle_factor, PrimeFieldElement* out1, PrimeFieldElement* out2) {
    if (GetModulus().NumLeadingZeros() < 2) {
      FieldElementBase<PrimeFieldElement>::FftButterfly(in1, in2, twiddle_factor, out1, out2);
      return;
    }

    constexpr auto kModulesTimesTwo = GetModulus() + GetModulus();
    const auto mul_res = UnreducedMontgomeryMul(in2.value_, twiddle_factor.value_);
    const auto tmp = ValueType::ReduceIfNeeded(in1.value_, kModulesTimesTwo);

    // We write out2 first because out1 may alias in1.
    *out2 = PrimeFieldElement(tmp + kModulesTimesTwo - mul_res);
    *out1 = PrimeFieldElement(tmp + mul_res);
  }

  static void FftNormalize(PrimeFieldElement* val) {
    if (GetModulus().NumLeadingZeros() < 2) {
      FieldElementBase<PrimeFieldElement>::FftNormalize(val);
      return;
    }
    *val = PrimeFieldElement(ValueType::ReduceIfNeeded(
        ValueType::ReduceIfNeeded(val->value_, GetModulus() + GetModulus()), GetModulus()));
  }

 private:
  explicit constexpr PrimeFieldElement(ValueType val) : value_(val) {}
  static PrimeFieldElement InverseToMontgomery(const ValueType& val) {
    return PrimeFieldElement(MontgomeryMul(val, kBigPrimeConstants::kMontgomeryRCubed));
  }

  template <bool IsConstexpr = false>
  static constexpr ValueType UnreducedMontgomeryMul(const ValueType& x, const ValueType& y) {
    return ValueType::MontMul(x, y, GetModulus(), kBigPrimeConstants::kMontgomeryMPrime);
  }

  template <bool IsConstexpr = false>
  static constexpr ValueType MontgomeryMul(const ValueType& x, const ValueType& y) {
    return ValueType::template ReduceIfNeeded<IsConstexpr>(
        UnreducedMontgomeryMul<IsConstexpr>(x, y), GetModulus());
  }

  ValueType value_;

  static_assert(
      !GetModulus().IsMsbSet(), "The implementation assumes that the msb of the modulus is 0");
};

}  // namespace starkware

#include "starkware/algebra/fields/prime_field_element.inl"

#endif  // STARKWARE_ALGEBRA_FIELDS_PRIME_FIELD_ELEMENT_H_
