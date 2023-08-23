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

// The purpose of this file is to allow invoking functions with template implementation from a
// context where the template parameter is unknown at compile time.

// The main function is InvokeGenericTemplateVersion - this function invokes the template version of
// a given function given a chooser function that decides which template parameter to use.

// A special case is that of writing functions for polymorphic field types
// (Field, FieldElement, ...) that hide a template implementation under the hood (TestFieldElement,
// LongFieldElement, ...).

// Consider, for example, a template function:
//   template <typename T> T Pow(const T& x, size_t exp) { ... }

// How can you call it, when you have an instance of FieldElement? (note that, for performance
// reasons, we want T to be an instance of the non-polymorphic class, such as TestFieldElement).

// This can be done using InvokeFieldTemplateVersion() as follows:

//   return InvokeFieldTemplateVersion(
//       [&](auto field_tag) {
//         using FieldElementT = typename decltype(field_tag)::type;
//         // FieldElementT is now the underlying type (say, TestFieldElement).
//         return FieldElement(Pow<FieldElementT>(x.as<FieldElementT>(), exp));
//       },
//       x.GetField());

// The first argument of InvokeFieldTemplateVersion() should be a lambda function with one argument
// of type auto. This argument is used for Tag Dispatching: it is an instance of TagType<T> where T
// is the underlying field and it allows accessing T using the expression: typename
// decltype(field_tag)::type.

// To use this functionality, follow the following pattern:
// 1. Define a template version of the function in a *.cc file, in an anonymous namespace.
// 2. Define the polymorphic version of the function in the same file, as a short function that only
// calls InvokeFieldTemplateVersion() with a lambda function that converts the parameters to the
// underlying types and calls the template version.
// 3. The header should only mention the polynomrphic version.

// See invoke_template_version_test.cc for an example.

// The general case (non field types) can be invoked like this:

//   return InvokeGenericTemplateVersion<InvokedTypes<uint64_t, uint32_t>>(
//       /*func=*/[&](auto type_tag) {
//         using IntegerT = typename decltype(type_tag)::type;
//         Serialize<IntegerT>(IntegerT());
//       },
//       /*chooser_func=*/[&](auto type_tag) {
//         using IntegerT = typename decltype(type_tag)::type;
//         return sizeof(IntegerT) == 4;
//       });

// The chooser_func will be called for the two given types (uint64_t and uint32_t).
// Since the first time it returns true is for uint32_t, func will be called for uint32_t.

#ifndef STARKWARE_ALGEBRA_UTILS_INVOKE_TEMPLATE_VERSION_H_
#define STARKWARE_ALGEBRA_UTILS_INVOKE_TEMPLATE_VERSION_H_

#include "glog/logging.h"

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polymorphic/field.h"

namespace starkware {

namespace invoke_template_version {
namespace details {

template <typename T>
struct TagType {
  using type = T;
};

}  // namespace details
}  // namespace invoke_template_version

template <typename... Types>
struct InvokedTypes;

template <>
struct InvokedTypes<> {
  static constexpr bool kIsEnd = true;
};

template <typename FirstTyp, typename... Types>
struct InvokedTypes<FirstTyp, Types...> {
  using FirstType = FirstTyp;
  using Next = InvokedTypes<Types...>;
  static constexpr bool kIsEnd = false;
};

template <typename InvokedTypesType, typename Func, typename ChooserFunc>
auto InvokeGenericTemplateVersion(const Func& func, const ChooserFunc& chooser_func) {
  using FirstType = typename InvokedTypesType::FirstType;
  if (chooser_func(invoke_template_version::details::TagType<FirstType>{})) {
    return func(invoke_template_version::details::TagType<FirstType>{});
  }
  using NextType = typename InvokedTypesType::Next;
  // NOLINTNEXTLINE: clang tidy if constexpr bug.
  if constexpr (NextType::kIsEnd) {
    THROW_STARKWARE_EXCEPTION("InvokeGenericTemplateVersion(): Unexpected type.");
  } else {  // NOLINT
    return InvokeGenericTemplateVersion<NextType>(func, chooser_func);
  }
}

using FieldInvokedTypes = InvokedTypes<
    PrimeFieldElement<252, 0>, TestFieldElement, LongFieldElement,
    ExtensionFieldElement<LongFieldElement>, ExtensionFieldElement<PrimeFieldElement<252, 0>>,
    ExtensionFieldElement<TestFieldElement>, PrimeFieldElement<124, 5>, LongFieldElement>;

/*
  Invokes func(field_tag) where field_tag is of type TagType<T> and T is the underlying type of the
  given Field instance.
*/
template <typename Func>
auto InvokeFieldTemplateVersion(const Func& func, const Field& field) {
  return InvokeGenericTemplateVersion<FieldInvokedTypes>(func, [&](auto field_tag) {
    using FieldElementT = typename decltype(field_tag)::type;
    return field.IsOfType<FieldElementT>();
  });
}

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_UTILS_INVOKE_TEMPLATE_VERSION_H_
