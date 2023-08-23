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

#include "starkware/utils/json.h"

#include <sstream>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/utils/json_builder.h"

namespace starkware {
namespace {

using testing::ElementsAre;
using testing::HasSubstr;
using testing::UnorderedElementsAre;

/*
  Creates a simple Json object for the tests.
*/
JsonValue SampleJson() {
  JsonBuilder builder;
  builder["field"] = "TestField";
  auto fri = builder["stark"]["fri"];
  fri["int"] = 5;
  fri["string"] = "hello";
  fri["uint64"] = Pow2(40);
  fri["negativeInt"] = -5;
  fri["field_elements"]
      .Append(TestFieldElement::FromUint(0x17))
      .Append(TestFieldElement::FromUint(0x80));
  fri["int_array"].Append(10).Append(20).Append(30);
  fri["bools"].Append(true).Append(false).Append(true);
  JsonBuilder sub;
  sub["array"].Append(5).Append(8).Append("str").Append("another str");
  JsonValue json_sub = sub.Build();
  fri["mixed_array"].Append(0).Append("string").Append(json_sub);
  fri["int_as_string"] = std::to_string(Pow2(40));
  return builder.Build();
}

class JsonTest : public testing::Test {
 public:
  JsonValue root = SampleJson();
};

TEST_F(JsonTest, ObjectAccess) {
  EXPECT_ASSERT(
      root["stark"]["fri2"]["int"], HasSubstr("Missing configuration object: /stark/fri2/"));
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"]["test"],
      HasSubstr("Configuration at /stark/fri/int/ is expected to be an object."));
}

TEST_F(JsonTest, ArrayAccess) {
  EXPECT_EQ(20U, root["stark"]["fri"]["int_array"][1].AsSizeT());
  EXPECT_ASSERT(root[1], HasSubstr("Configuration at / is expected to be an array."));
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"][1],
      HasSubstr("Configuration at /stark/fri/int/ is expected to be an array."));
}

TEST_F(JsonTest, HasValue) {
  EXPECT_TRUE(root["stark"].HasValue());
  EXPECT_FALSE(root["stark2"].HasValue());
}

TEST_F(JsonTest, AsBool) {
  EXPECT_EQ(true, root["stark"]["fri"]["bools"][0].AsBool());
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"].AsBool(),
      HasSubstr("Configuration at /stark/fri/int/ is expected to be a boolean."));
}

TEST_F(JsonTest, AsUint64) {
  EXPECT_EQ(Pow2(40), root["stark"]["fri"]["uint64"].AsUint64());
  EXPECT_EQ(5, root["stark"]["fri"]["int"].AsUint64());
  EXPECT_ASSERT(
      root["stark"]["fri"]["string"].AsUint64(),
      HasSubstr("Configuration at /stark/fri/string/ is expected to be a uint64."));
  EXPECT_ASSERT(
      root["stark"]["fri"]["negativeInt"].AsUint64(),
      HasSubstr("Configuration at /stark/fri/negativeInt/ is expected to be a uint64."));
}

TEST_F(JsonTest, AsUint64FromString) {
  EXPECT_EQ(Pow2(40), root["stark"]["fri"]["int_as_string"].AsUint64FromString());
  EXPECT_ASSERT(
      root["stark"]["fri"]["string"].AsUint64FromString(),
      HasSubstr("does not represent a valid uint64_t value"));
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"].AsUint64FromString(),
      HasSubstr("Configuration at /stark/fri/int/ is expected to be a string."));
}

TEST_F(JsonTest, AsBoolVector) {
  EXPECT_THAT(root["stark"]["fri"]["bools"].AsBoolVector(), ElementsAre(true, false, true));
}

TEST_F(JsonTest, AsSizeT) {
  EXPECT_EQ(5U, root["stark"]["fri"]["int"].AsSizeT());
  EXPECT_ASSERT(
      root["stark"]["fri"]["missing"].AsSizeT(),
      HasSubstr("Missing configuration value: /stark/fri/missing/"));
  EXPECT_ASSERT(
      root["stark"]["fri"]["string"].AsSizeT(),
      HasSubstr("Configuration at /stark/fri/string/ is expected to be an integer."));
}

TEST_F(JsonTest, ArrayLength) {
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"].ArrayLength(),
      HasSubstr("Configuration at /stark/fri/int/ is expected to be an array."));
  EXPECT_EQ(3U, root["stark"]["fri"]["int_array"].ArrayLength());
  EXPECT_EQ(3U, root["stark"]["fri"]["mixed_array"].ArrayLength());
  EXPECT_EQ(4U, root["stark"]["fri"]["mixed_array"][2]["array"].ArrayLength());
}

TEST_F(JsonTest, AsSizeTVector) {
  EXPECT_THAT(root["stark"]["fri"]["int_array"].AsSizeTVector(), ElementsAre(10, 20, 30));
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"].AsSizeTVector(),
      HasSubstr("Configuration at /stark/fri/int/ is expected to be an array."));
  EXPECT_ASSERT(
      root["stark"]["fri"]["mixed_array"].AsSizeTVector(),
      HasSubstr("Configuration at /stark/fri/mixed_array/1/ is expected to be an integer."));
}

TEST_F(JsonTest, AsString) {
  EXPECT_EQ("hello", root["stark"]["fri"]["string"].AsString());
  EXPECT_ASSERT(
      root["stark"]["fri"]["int"].AsString(),
      HasSubstr("Configuration at /stark/fri/int/ is expected to be a string."));
}

TEST_F(JsonTest, AsField) {
  EXPECT_TRUE(root["field"].AsField().IsOfType<TestFieldElement>());
  EXPECT_ASSERT(root["stark"]["fri"]["string"].AsField(), HasSubstr("Undefined field 'hello'."));
}

TEST_F(JsonTest, AsFieldElement) {
  EXPECT_EQ(
      TestFieldElement::FromUint(0x17),
      root["stark"]["fri"]["field_elements"][0].AsFieldElement<TestFieldElement>());
  EXPECT_ASSERT(
      root["stark"]["fri"]["string"].AsFieldElement<TestFieldElement>(),
      HasSubstr("does not start with '0x'"));
}

TEST_F(JsonTest, AsFieldElementVector) {
  EXPECT_THAT(
      root["stark"]["fri"]["field_elements"].AsFieldElementVector<TestFieldElement>(),
      ElementsAre(TestFieldElement::FromUint(0x17), TestFieldElement::FromUint(0x80)));
}

TEST_F(JsonTest, Keys) {
  EXPECT_THAT(
      root["stark"]["fri"].Keys(),
      UnorderedElementsAre(
          "bools", "field_elements", "int", "int_array", "int_as_string", "mixed_array",
          "negativeInt", "string", "uint64"));
}

TEST_F(JsonTest, Equality) {
  EXPECT_TRUE(
      JsonValue::FromString("{\"x\":4, \"y\": {\"z\":[3 ,2]}}") ==
      JsonValue::FromString("{\"x\":4,\"y\":{\"z\":[3,2]}}"));
  EXPECT_FALSE(
      JsonValue::FromString("{\"xyz\":4, \"y\": {\"z\":[3 ,2]}}") ==
      JsonValue::FromString("{\"x\":4,\"y\":{\"z\":[3,2]}}"));
  EXPECT_FALSE(
      JsonValue::FromString("{\"x\":4, \"y\": {\"z\":[3 ,2]}}") !=
      JsonValue::FromString("{\"x\":4,\"y\":{\"z\":[3,2]}}"));
  EXPECT_TRUE(
      JsonValue::FromString("{\"x\":4, \"y\": {\"z\":[3 ,2]}}") !=
      JsonValue::FromString("{\"x\":4,\"y\":{\"z\":[5,2]}}"));
}

TEST_F(JsonTest, JsonBuilderFromJsonValue) {
  JsonBuilder builder = JsonBuilder::FromJsonValue(root);
  ASSERT_EQ(builder.Build(), root);

  builder["field"] = "other value";
  ASSERT_EQ(builder.Build()["field"].AsString(), "other value");
  // Original value should not change.
  ASSERT_EQ(root["field"].AsString(), "TestField");
}

}  // namespace
}  // namespace starkware
