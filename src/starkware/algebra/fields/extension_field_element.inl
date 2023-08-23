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

#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"

namespace starkware {

template <typename FieldElementT>
ExtensionFieldElement<FieldElementT> ExtensionFieldElement<FieldElementT>::operator+(
    const ExtensionFieldElement<FieldElementT>& rhs) const {
  return {coef0_ + rhs.coef0_, coef1_ + rhs.coef1_};
}

template <typename FieldElementT>
ExtensionFieldElement<FieldElementT> ExtensionFieldElement<FieldElementT>::operator-(
    const ExtensionFieldElement<FieldElementT>& rhs) const {
  return {coef0_ - rhs.coef0_, coef1_ - rhs.coef1_};
}

template <typename FieldElementT>
ALWAYS_INLINE ExtensionFieldElement<FieldElementT> ExtensionFieldElement<FieldElementT>::operator*(
    const ExtensionFieldElement<FieldElementT>& rhs) const {
  if (rhs.coef1_ == FieldElementT::Zero()) {
    if (coef1_ == FieldElementT::Zero()) {
      return {coef0_ * rhs.coef0_, FieldElementT::Zero()};
    }
    return {coef0_ * rhs.coef0_, coef1_ * rhs.coef0_};
  }
  if (coef1_ == FieldElementT::Zero()) {
    return {coef0_ * rhs.coef0_, coef0_ * rhs.coef1_};
  }
  return {coef0_ * rhs.coef0_ + coef1_ * rhs.coef1_ * FieldElementT::Generator(),
          coef0_ * rhs.coef1_ + coef1_ * rhs.coef0_};
}

template <typename FieldElementT>
bool ExtensionFieldElement<FieldElementT>::operator==(
    const ExtensionFieldElement<FieldElementT>& rhs) const {
  return coef0_ == rhs.coef0_ && coef1_ == rhs.coef1_;
}

template <typename FieldElementT>
ExtensionFieldElement<FieldElementT> ExtensionFieldElement<FieldElementT>::Inverse() const {
  ASSERT_RELEASE(
      coef0_ != FieldElementT::Zero() || coef1_ != FieldElementT::Zero(),
      "Zero does not have an inverse");
  const auto denom = coef0_ * coef0_ - coef1_ * coef1_ * FieldElementT::Generator();
  const auto denom_inv = denom.Inverse();
  return {coef0_ * denom_inv, -coef1_ * denom_inv};
}

template <typename FieldElementT>
void ExtensionFieldElement<FieldElementT>::ToBytes(
    gsl::span<std::byte> span_out, bool use_big_endian) const {
  coef0_.ToBytes(span_out.subspan(0, FieldElementT::SizeInBytes()), use_big_endian);
  coef1_.ToBytes(
      span_out.subspan(FieldElementT::SizeInBytes(), FieldElementT::SizeInBytes()), use_big_endian);
}

template <typename FieldElementT>
ExtensionFieldElement<FieldElementT> ExtensionFieldElement<FieldElementT>::FromBytes(
    gsl::span<const std::byte> bytes, bool use_big_endian) {
  return ExtensionFieldElement(
      FieldElementT::FromBytes(bytes.subspan(0, FieldElementT::SizeInBytes()), use_big_endian),
      FieldElementT::FromBytes(
          bytes.subspan(FieldElementT::SizeInBytes(), FieldElementT::SizeInBytes()),
          use_big_endian));
}

template <typename FieldElementT>
std::string ExtensionFieldElement<FieldElementT>::ToString() const {
  std::string str0 = coef0_.FieldElementT::ToString();
  std::string str1 = coef1_.FieldElementT::ToString();
  return str0 + "::" + str1;
}

template <typename FieldElementT>
ExtensionFieldElement<FieldElementT> ExtensionFieldElement<FieldElementT>::FromString(
    const std::string& s) {
  size_t split_point = s.find("::");
  // When converting a base field element string to an extension field, coef1 might not be mentioned
  // in the string, so the entire string is coef0.
  if (split_point == std::string::npos) {
    return ExtensionFieldElement(FieldElementT::FromString(s), FieldElementT::Zero());
  }
  return ExtensionFieldElement(
      FieldElementT::FromString(s.substr(0, split_point)),
      FieldElementT::FromString(s.substr(split_point + 2, s.length())));
}

template <>
constexpr auto ExtensionFieldElement<TestFieldElement>::ConstexprFromBigInt(const BigInt<1>& val)
    -> ExtensionFieldElement<TestFieldElement> {
  return ExtensionFieldElement(
      TestFieldElement::ConstexprFromBigInt(val), TestFieldElement::Zero());
}

// The following Generator() and PrimeFactors() functions are field specific definitions.

template <>
inline auto ExtensionFieldElement<TestFieldElement>::Generator() -> ExtensionFieldElement {
  return ExtensionFieldElement(TestFieldElement::FromUint(8), TestFieldElement::FromUint(1));
}

template <>
inline auto ExtensionFieldElement<PrimeFieldElement<252, 0>>::Generator() -> ExtensionFieldElement {
  return ExtensionFieldElement(
      PrimeFieldElement<252, 0>::FromUint(8), PrimeFieldElement<252, 0>::FromUint(1));
}

template <>
inline auto ExtensionFieldElement<LongFieldElement>::Generator() -> ExtensionFieldElement {
  return ExtensionFieldElement(LongFieldElement::FromUint(3), LongFieldElement::FromUint(1));
}

template <typename FieldElementT>
inline auto ExtensionFieldElement<FieldElementT>::Generator() -> ExtensionFieldElement {
  ASSERT_RELEASE(false, "ExtensionFieldElement is unsupported over this field.");
}

template <>
constexpr auto ExtensionFieldElement<TestFieldElement>::PrimeFactors() {
  return std::array<BigInt<1>, 4>{0x2_Z, 0x3_Z, 0x4f_Z, 0x13716af_Z};
}

template <>
constexpr auto ExtensionFieldElement<PrimeFieldElement<252, 0>>::PrimeFactors() {
  return std::array<BigInt<3>, 11>{0x2_Z,
                                   0x3_Z,
                                   0x5_Z,
                                   0x7_Z,
                                   0xd_Z,
                                   0x17_Z,
                                   0x1d7ae1_Z,
                                   0x5e2430d_Z,
                                   0x9f1e667_Z,
                                   0xaaf5b07_Z,
                                   0xed8329a1355f01889da81e879a9d4afdb4b13e60463e5817_Z};
}

template <>
constexpr auto ExtensionFieldElement<LongFieldElement>::PrimeFactors() {
  return std::array<BigInt<1>, 10>{0x2_Z,  0x3_Z,   0x7_Z,   0xd_Z,      0xa7_Z,
                                   0xd3_Z, 0x125_Z, 0x1c9_Z, 0x52be0f_Z, 0x1520bdb_Z};
}

}  // namespace starkware
