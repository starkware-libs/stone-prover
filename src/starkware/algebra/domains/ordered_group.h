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

#ifndef STARKWARE_ALGEBRA_DOMAINS_ORDERED_GROUP_H_
#define STARKWARE_ALGEBRA_DOMAINS_ORDERED_GROUP_H_

#include <vector>

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

/*
  Represents a group with ordering of the elements.
*/
class OrderedGroup {
 public:
  virtual ~OrderedGroup() = default;
  virtual size_t Size() const = 0;
  virtual Field GetField() const = 0;

  /*
    Returns the index'th element in the group.
    Index must satisfy 0 <= index < Size() - otherwise, an assertion is invoked.
  */
  virtual FieldElement ElementByIndex(size_t index) const = 0;

  virtual bool operator==(const OrderedGroup& other) const = 0;

  bool operator!=(const OrderedGroup& other) const { return !(*this == other); }
};

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_DOMAINS_ORDERED_GROUP_H_
