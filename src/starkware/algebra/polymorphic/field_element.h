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

#ifndef STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_ELEMENT_H_
#define STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_ELEMENT_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

class Field;

class FieldElement {
 public:
  template <typename T>
  explicit FieldElement(const T& t);

  FieldElement(const FieldElement& other);

  FieldElement& operator=(const FieldElement& other) &;

  ~FieldElement() = default;
  FieldElement(FieldElement&& other) = default;
  FieldElement& operator=(FieldElement&& other) & = default;

  Field GetField() const;

  FieldElement operator+(const FieldElement& other) const;
  FieldElement operator-(const FieldElement& other) const;
  FieldElement operator-() const;
  FieldElement operator*(const FieldElement& other) const;
  FieldElement operator/(const FieldElement& other) const;
  FieldElement Inverse() const;
  FieldElement Pow(uint64_t exp) const;

  void ToBytes(gsl::span<std::byte> span_out, bool use_big_endian = true) const;
  bool operator==(const FieldElement& other) const;
  bool operator!=(const FieldElement& other) const;

  size_t SizeInBytes() const;

  std::string ToString() const;
  friend std::ostream& operator<<(std::ostream& out, const FieldElement& field_element);

  // Asserts that the underlying type is T, and returns the underlying value.
  template <typename T>
  const T& As() const;

 private:
  class WrapperBase;

  template <typename T>
  class Wrapper;

  std::unique_ptr<WrapperBase> wrapper_;
};

}  // namespace starkware

#include "starkware/algebra/polymorphic/field_element.inl"

#endif  // STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_ELEMENT_H_
