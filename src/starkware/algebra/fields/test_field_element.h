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

#ifndef STARKWARE_ALGEBRA_FIELDS_TEST_FIELD_ELEMENT_H_
#define STARKWARE_ALGEBRA_FIELDS_TEST_FIELD_ELEMENT_H_

#include <array>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "starkware/algebra/big_int.h"
#include "starkware/algebra/field_element_base.h"
#include "starkware/randomness/prng.h"

namespace starkware {

using std::uint32_t;

class TestFieldElement : public FieldElementBase<TestFieldElement> {
 public:
  // Chosen for fft considerations.
  static constexpr uint64_t kModulus = (3 * (1UL << 30) + 1);

#ifdef NDEBUG
  // We allow the use of the default constructor only in Release builds in order to reduce
  // memory allocation time for vectors of field elements.
  TestFieldElement() = default;
#else
  // In debug builds, we make sure that the default constructor is not called at all.
  TestFieldElement() = delete;
#endif

  static TestFieldElement Uninitialized() {
    return Zero();
  }

  static constexpr TestFieldElement FromUint(uint64_t val) {
    return TestFieldElement(val % kModulus);
  }

  static constexpr TestFieldElement FromBigInt(const BigInt<1>& val) { return FromUint(val[0]); }

  static constexpr TestFieldElement ConstexprFromBigInt(const BigInt<1>& val) {
    // A wrapper function for consistency with PrimeFieldElement::ConstexprFromBigInt.
    // Auto-generated code uses ConstexprFromBigInt which will not compile with TestFieldElement.
    return FromBigInt(val);
  }

  TestFieldElement operator+(const TestFieldElement& rhs) const {
    return FromUint(static_cast<uint64_t>(value_) + static_cast<uint64_t>(rhs.value_));
  }

  TestFieldElement operator-(const TestFieldElement& rhs) const {
    return FromUint(static_cast<uint64_t>(value_) + kModulus - static_cast<uint64_t>(rhs.value_));
  }

  TestFieldElement operator-() const { return FromUint(kModulus - static_cast<uint64_t>(value_)); }

  TestFieldElement operator*(const TestFieldElement& rhs) const {
    return FromUint(static_cast<uint64_t>(value_) * static_cast<uint64_t>(rhs.value_));
  }

  bool operator==(const TestFieldElement& rhs) const { return value_ == rhs.value_; }

  TestFieldElement Inverse() const;

  // Constructs a byte serialization of the field element at the given span.
  void ToBytes(gsl::span<std::byte> span_out, bool use_big_endian = true) const;

  static constexpr TestFieldElement Zero() { return TestFieldElement(0); }

  static constexpr TestFieldElement One() { return TestFieldElement(1); }

  static TestFieldElement RandomElement(PrngBase* prng);

  static TestFieldElement FromBytes(gsl::span<const std::byte> bytes, bool use_big_endian = true);

  static TestFieldElement FromString(const std::string& s);

  std::string ToString() const;

  static constexpr BigInt<1> FieldSize() { return BigInt<1>(kModulus); }
  static constexpr TestFieldElement Generator() { return TestFieldElement(5); }
  static constexpr std::array<BigInt<1>, 2> PrimeFactors() { return {BigInt<1>(2), BigInt<1>(3)}; }
  static constexpr size_t SizeInBytes() { return sizeof(uint32_t); }

 private:
  explicit constexpr TestFieldElement(uint32_t val) : value_(val) {}

  uint32_t value_ = 0;
};

}  // namespace starkware

#include "starkware/algebra/fields/test_field_element.inl"

#endif  // STARKWARE_ALGEBRA_FIELDS_TEST_FIELD_ELEMENT_H_
