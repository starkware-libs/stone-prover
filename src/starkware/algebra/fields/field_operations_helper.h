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

#ifndef STARKWARE_ALGEBRA_FIELDS_FIELD_OPERATIONS_HELPER_H_
#define STARKWARE_ALGEBRA_FIELDS_FIELD_OPERATIONS_HELPER_H_

#include <functional>

#include "starkware/algebra/polymorphic/field.h"

namespace starkware {

/*
  Given a field, returns true iff it is of type ExtensionFieldElement<FieldElementT> (for some
  FieldElementT).
*/
bool IsExtensionField(const Field& field);

template <typename FieldElementT>
bool IsExtensionField();

FieldElement GetFrobenius(const FieldElement& elm);

/*
  Returns the result of Frobenius automorphism on a given field element. E.g., in case of a prime
  field, it is the field element itself. Otherwise, it is a conjugate of the field element.
*/
template <typename FieldElementT>
FieldElementT GetFrobenius(const FieldElementT& elm);
template <typename FieldElementT>
auto AsBaseFieldElement(FieldElementT elm);

}  // namespace starkware

#include "starkware/algebra/fields/field_operations_helper.inl"

#endif  // STARKWARE_ALGEBRA_FIELDS_FIELD_OPERATIONS_HELPER_H_
