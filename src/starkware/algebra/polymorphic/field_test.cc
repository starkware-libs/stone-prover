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

#include "starkware/algebra/polymorphic/field.h"

#include "gtest/gtest.h"

#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

TEST(Field, BasicTest) {
  Field f = Field::Create<TestFieldElement>();
  EXPECT_EQ(TestFieldElement::FromUint(0), f.Zero().As<TestFieldElement>());
  EXPECT_EQ(TestFieldElement::FromUint(1), f.One().As<TestFieldElement>());

  // Check that the following code compiles.
  Prng prng;
  f.RandomElement(&prng);
}

TEST(Field, Serialization) {
  Field f = Field::Create<TestFieldElement>();
  std::array<std::byte, 4> data{};
  f.One().ToBytes(data, true);
  EXPECT_EQ(BytesToHexString(data, false), "0x00000001");
  EXPECT_EQ(f.FromBytes(data, true), f.One());
  EXPECT_NE(f.FromBytes(data, false), f.One());
  f.One().ToBytes(data, false);
  EXPECT_EQ(BytesToHexString(data, false), "0x01000000");
  EXPECT_EQ(f.FromBytes(data, false), f.One());
}

TEST(Field, IsOfType) {
  Field f = Field::Create<TestFieldElement>();
  EXPECT_TRUE(f.IsOfType<TestFieldElement>());
  EXPECT_FALSE(f.IsOfType<LongFieldElement>());
}

TEST(Field, Equality) {
  Field field1 = Field::Create<TestFieldElement>();
  Field field2 = Field::Create<TestFieldElement>();
  Field field3 = Field::Create<LongFieldElement>();
  EXPECT_TRUE(field1 == field2);
  EXPECT_FALSE(field1 != field2);
  EXPECT_FALSE(field1 == field3);
  EXPECT_TRUE(field1 != field3);
}

}  // namespace
}  // namespace starkware
