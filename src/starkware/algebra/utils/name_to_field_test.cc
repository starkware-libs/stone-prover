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

#include "starkware/algebra/utils/name_to_field.h"

#include "gtest/gtest.h"

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"

namespace starkware {
namespace {

TEST(NameToField, BasicTest) {
  EXPECT_TRUE(NameToField("TestField"));
  EXPECT_TRUE(NameToField("LongField"));
  EXPECT_TRUE(NameToField("PrimeField0"));
  EXPECT_TRUE(NameToField("PrimeField1"));
  EXPECT_TRUE(NameToField("PrimeField2"));
  EXPECT_TRUE(NameToField("PrimeField3"));
  EXPECT_TRUE(NameToField("PrimeField4"));
  EXPECT_TRUE(NameToField("ExtensionTestField"));
  EXPECT_TRUE(NameToField("ExtensionLongField"));
  EXPECT_TRUE(NameToField("ExtensionPrimeField0"));

  EXPECT_FALSE(NameToField("BloomField"));

  EXPECT_EQ(Field::Create<TestFieldElement>(), NameToField("TestField"));
  EXPECT_EQ(Field::Create<LongFieldElement>(), NameToField("LongField"));
  EXPECT_EQ((Field::Create<PrimeFieldElement<252, 0>>()), NameToField("PrimeField0"));
  EXPECT_EQ((Field::Create<PrimeFieldElement<254, 1>>()), NameToField("PrimeField1"));
  EXPECT_EQ((Field::Create<PrimeFieldElement<254, 2>>()), NameToField("PrimeField2"));
  EXPECT_EQ((Field::Create<PrimeFieldElement<252, 3>>()), NameToField("PrimeField3"));
  EXPECT_EQ((Field::Create<PrimeFieldElement<255, 4>>()), NameToField("PrimeField4"));
  EXPECT_EQ(
      (Field::Create<ExtensionFieldElement<TestFieldElement>>()),
      NameToField("ExtensionTestField"));
  EXPECT_EQ(
      (Field::Create<ExtensionFieldElement<LongFieldElement>>()),
      NameToField("ExtensionLongField"));
  EXPECT_EQ(
      (Field::Create<ExtensionFieldElement<PrimeFieldElement<252, 0>>>()),
      NameToField("ExtensionPrimeField0"));
}

}  // namespace
}  // namespace starkware
