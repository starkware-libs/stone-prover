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

#ifndef STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_H_
#define STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "starkware/randomness/prng.h"

namespace starkware {

class FieldElement;

class Field {
 public:
  template <typename FieldElementT>
  static Field Create();

  FieldElement One() const;
  FieldElement Zero() const;
  FieldElement Generator() const;
  FieldElement RandomElement(PrngBase* prng) const;
  FieldElement FromBytes(gsl::span<const std::byte> bytes, bool use_big_endian = true) const;
  FieldElement FromString(const std::string& s) const;
  size_t ElementSizeInBytes() const;
  bool operator==(const Field& other) const;
  bool operator!=(const Field& other) const;

  /*
    Returns true if the underlying type is T, and false otherwise.
    For example:
      Field f = ...
      LOG(INFO) << f.IsOfType<TestFieldElement>();
  */
  template <typename FieldElementT>
  bool IsOfType() const;

 private:
  class FieldWrapperBase;

  template <typename FieldElementT>
  class FieldWrapper;

  explicit Field(const std::shared_ptr<FieldWrapperBase>& t) : wrapper_(t) {}

  std::shared_ptr<const FieldWrapperBase> wrapper_;
};

}  // namespace starkware

#include "starkware/algebra/polymorphic/field.inl"

#endif  // STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_H_
