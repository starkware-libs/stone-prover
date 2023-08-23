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

#ifndef STARKWARE_ALGEBRA_DOMAINS_MULTIPLICATIVE_GROUP_H_
#define STARKWARE_ALGEBRA_DOMAINS_MULTIPLICATIVE_GROUP_H_

#include <vector>

#include "starkware/algebra/domains/ordered_group.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

/*
  MultiplicativeGroup class represents a cyclic multiplicative subgroup of the multiplicative group
  of the field. An instance of this class is generated using the static MakeGroup method, which
  given group size generates a group of the given size.
*/
class MultiplicativeGroup : public OrderedGroup {
 public:
  static MultiplicativeGroup MakeGroup(size_t size, const Field& field);

  const FieldElement& Generator() const { return generator_; }

  size_t Size() const override { return size_; }

  Field GetField() const override { return generator_.GetField(); }

  /*
    The group elements are ordered in a natural order. Given a generator of the group (returned by
    the Generator() method), g, the i-th element is g^i.
    Index must satisfy 0 <= index < Size() - otherwise, an assertion is invoked.
  */
  FieldElement ElementByIndex(size_t index) const override {
    ASSERT_RELEASE(index < Size(), "Index out of range.");
    return generator_.Pow(index);
  }

  bool operator==(const OrderedGroup& other) const override {
    const auto* multiplicative_group = dynamic_cast<const MultiplicativeGroup*>(&other);
    return multiplicative_group != nullptr && *this == *multiplicative_group;
  }

  bool operator==(const MultiplicativeGroup& other) const {
    return (Size() == other.Size()) && (Generator() == other.Generator());
  }

  bool operator!=(const MultiplicativeGroup& other) const { return !(*this == other); }

 private:
  MultiplicativeGroup(size_t size, FieldElement generator);

  size_t size_;
  FieldElement generator_;
};

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_DOMAINS_MULTIPLICATIVE_GROUP_H_
