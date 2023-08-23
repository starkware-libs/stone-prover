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

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/error_handling/error_handling.h"

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

class Field::FieldWrapperBase {
 public:
  virtual ~FieldWrapperBase() = default;
  virtual FieldElement One() const = 0;
  virtual FieldElement Zero() const = 0;
  virtual FieldElement Generator() const = 0;
  virtual FieldElement RandomElement(PrngBase* prng) const = 0;
  virtual FieldElement FromBytes(gsl::span<const std::byte> bytes, bool use_big_endian) const = 0;
  virtual FieldElement FromString(const std::string& s) const = 0;
  virtual size_t ElementSizeInBytes() const = 0;
  virtual bool Equals(const Field& other) const = 0;
};

template <typename FieldElementT>
class Field::FieldWrapper : public FieldWrapperBase {
 public:
  FieldElement One() const override { return FieldElement(FieldElementT::One()); }

  FieldElement Zero() const override { return FieldElement(FieldElementT::Zero()); }

  FieldElement Generator() const override { return FieldElement(FieldElementT::Generator()); }

  FieldElement RandomElement(PrngBase* prng) const override {
    return FieldElement(FieldElementT::RandomElement(prng));
  }

  FieldElement FromBytes(gsl::span<const std::byte> bytes, bool use_big_endian) const override {
    ASSERT_DEBUG(
        bytes.size() == FieldElementT::SizeInBytes(),
        "Wrong number of bytes provided for FieldElement deserialization");

    return FieldElement(FieldElementT::FromBytes(bytes, use_big_endian));
  }

  FieldElement FromString(const std::string& s) const override {
    return FieldElement(FieldElementT::FromString(s));
  }

  size_t ElementSizeInBytes() const override { return FieldElementT::SizeInBytes(); }

  bool Equals(const Field& other) const override { return other.IsOfType<FieldElementT>(); }
};

template <typename FieldElementT>
Field Field::Create() {
  return Field(std::make_shared<FieldWrapper<FieldElementT>>());
}

template <typename FieldElementT>
bool Field::IsOfType() const {
  return dynamic_cast<const FieldWrapper<FieldElementT>*>(wrapper_.get()) != nullptr;
}

}  // namespace starkware
