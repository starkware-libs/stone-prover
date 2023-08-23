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

#include "starkware/algebra/fields/field_operations_helper.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/algebra/fields/fraction_field_element.h"
#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

template <typename T>
class BaseFieldTest : public ::testing::Test {
 public:
  Prng prng;
};

using BaseFieldTypes =
    ::testing::Types<TestFieldElement, LongFieldElement, PrimeFieldElement<252, 0>>;
TYPED_TEST_CASE(BaseFieldTest, BaseFieldTypes);

// ------ Tests ------

TYPED_TEST(BaseFieldTest, PolymorphicGetFrobenius) {
  using FieldElementT = TypeParam;
  const FieldElement elm = FieldElement(FieldElementT::RandomElement(&(this->prng)));
  EXPECT_ASSERT(GetFrobenius(elm), testing::HasSubstr("GetFrobenius() is not implemented"));
}

TYPED_TEST(BaseFieldTest, AsBaseFieldElementFromBase) {
  using FieldElementT = TypeParam;
  const FieldElementT elm = FieldElementT::RandomElement(&(this->prng));
  EXPECT_EQ(AsBaseFieldElement(elm), elm);
}

TYPED_TEST(BaseFieldTest, IsExtensionField) { EXPECT_FALSE(IsExtensionField<TypeParam>()); }

// ------ Definitions for extension field tests ------

template <typename T>
class ExtensionFieldTest : public ::testing::Test {
 public:
  Prng prng;
};

using BaseFieldForExtensionTypes =
    ::testing::Types<TestFieldElement, LongFieldElement, PrimeFieldElement<252, 0>>;
TYPED_TEST_CASE(ExtensionFieldTest, BaseFieldForExtensionTypes);

// ------ Tests ------

TYPED_TEST(ExtensionFieldTest, PolymorphicGetFrobenius) {
  using BaseField = TypeParam;
  using ExtensionFieldElementT = ExtensionFieldElement<BaseField>;
  const FieldElement elm = FieldElement(ExtensionFieldElementT::RandomElement(&(this->prng)));
  const FieldElement conj_elm = GetFrobenius(elm);
  const ExtensionFieldElementT templated_elm = elm.template As<ExtensionFieldElementT>();
  const ExtensionFieldElementT templated_conj_elm = conj_elm.template As<ExtensionFieldElementT>();
  EXPECT_EQ(templated_conj_elm, Pow(templated_elm, BaseField::FieldSize().ToBoolVector()));
}

TYPED_TEST(ExtensionFieldTest, AsBaseFieldElementFromExtension) {
  using ExtensionFieldElementT = ExtensionFieldElement<TypeParam>;
  const ExtensionFieldElementT elm = ExtensionFieldElementT::RandomBaseElement(&(this->prng));
  EXPECT_EQ(AsBaseFieldElement(elm), elm.GetCoef0());
}

TYPED_TEST(ExtensionFieldTest, IsExtensionField) {
  EXPECT_TRUE(IsExtensionField<ExtensionFieldElement<TypeParam>>());
}

}  // namespace
}  // namespace starkware
