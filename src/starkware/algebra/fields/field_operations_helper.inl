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

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/algebra/utils/invoke_template_version.h"

namespace starkware {

/*
  Using structs that wrap the functions is done to enable the specialization for extension field
  element (partial specialization in function is not allowed).
*/
template <typename FieldElementT>
struct FieldOperationsHelperFunctions {
  static bool IsExtensionField() { return false; }

  static FieldElementT GetFrobenius(const FieldElementT& /*elm*/) {
    ASSERT_RELEASE(
        false,
        "GetFrobenius() is not implemented in a field which is not of type "
        "ExtensionField<FieldElementT>.");
  }

  static FieldElementT AsBaseFieldElement(FieldElementT elm) { return elm; }
};

template <typename FieldElementT>
struct FieldOperationsHelperFunctions<ExtensionFieldElement<FieldElementT>> {
  static bool IsExtensionField() { return true; }

  static ExtensionFieldElement<FieldElementT> GetFrobenius(
      const ExtensionFieldElement<FieldElementT>& elm) {
    return {elm.GetCoef0(), -elm.GetCoef1()};
  }

  static FieldElementT AsBaseFieldElement(ExtensionFieldElement<FieldElementT> elm) {
    ASSERT_RELEASE(
        elm.InBaseField(), "Element is required to be in base field, i.e in the form (x,0).");
    return elm.GetCoef0();
  }
};

inline bool IsExtensionField(const Field& field) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) -> bool {
        using FieldElementT = typename decltype(field_tag)::type;
        return FieldOperationsHelperFunctions<FieldElementT>::IsExtensionField();
      },
      field);
}

template <typename FieldElementT>
bool IsExtensionField() {
  return FieldOperationsHelperFunctions<FieldElementT>::IsExtensionField();
}

template <typename FieldElementT>
auto AsBaseFieldElement(FieldElementT elm) {
  return FieldOperationsHelperFunctions<FieldElementT>::AsBaseFieldElement(elm);
}

inline FieldElement GetFrobenius(const FieldElement& elm) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) -> FieldElement {
        using FieldElementT = typename decltype(field_tag)::type;
        return FieldElement(FieldOperationsHelperFunctions<FieldElementT>::GetFrobenius(
            elm.template As<FieldElementT>()));
      },
      elm.GetField());
}

template <typename FieldElementT>
FieldElementT GetFrobenius(const FieldElementT& elm) {
  return FieldOperationsHelperFunctions<FieldElementT>::GetFrobenius(elm);
}

}  // namespace starkware
