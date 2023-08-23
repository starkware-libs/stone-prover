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

#include "starkware/algebra/polymorphic/field_element_vector.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::ElementsAre;
using testing::HasSubstr;

TEST(FieldElementVector, BasicTest) {
  FieldElementVector vec = FieldElementVector::Make<TestFieldElement>();
  vec.PushBack(FieldElement(TestFieldElement::FromUint(4)));
  vec.PushBack(FieldElement(TestFieldElement::FromUint(6)));
  ASSERT_EQ(2U, vec.Size());
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(4)), vec[0]);
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(6)), vec[1]);
  EXPECT_THAT(
      vec.As<TestFieldElement>(),
      ElementsAre(TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)));
}

TEST(FieldElementVector, MakeAndCopyFrom) {
  FieldElementVector vec = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)});
  EXPECT_THAT(
      vec.As<TestFieldElement>(),
      ElementsAre(TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)));
  FieldElementVector vec2 = FieldElementVector::CopyFrom(ConstFieldElementSpan(vec));
  EXPECT_THAT(
      vec2.As<TestFieldElement>(),
      ElementsAre(TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)));
}

TEST(FieldElementVector, MakeDoesNotCopy) {
  std::vector<TestFieldElement> vec({TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)});
  auto data_ptr = vec.data();
  auto field_element_vector = FieldElementVector::Make(std::move(vec));
  EXPECT_EQ(data_ptr, field_element_vector.As<TestFieldElement>().data());
}

TEST(FieldElementVector, MakeWithValue) {
  Prng prng;
  const size_t size = prng.UniformInt(0, 10);
  const TestFieldElement value = TestFieldElement::RandomElement(&prng);
  const std::vector<TestFieldElement> reference(size, value);
  const FieldElementVector result = FieldElementVector::Make(size, FieldElement(value));
  EXPECT_EQ(result.As<TestFieldElement>(), reference);
}

TEST(FieldElementVector, GetField) {
  FieldElementVector vec = FieldElementVector::Make<TestFieldElement>();
  EXPECT_TRUE(vec.GetField().IsOfType<TestFieldElement>());
}

TEST(FieldElementVector, Eq) {
  FieldElementVector vec1 = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)});
  FieldElementVector vec2 = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)});
  FieldElementVector vec3 = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(4), TestFieldElement::FromUint(20)});
  EXPECT_TRUE(vec1 == vec2);
  EXPECT_FALSE(vec1 != vec2);
  EXPECT_FALSE(vec1 == vec3);
  EXPECT_TRUE(vec1 != vec3);
}

TEST(FieldElementVector, MakeFieldElementVectorFromField) {
  Field f = Field::Create<TestFieldElement>();
  FieldElementVector vec = FieldElementVector::Make(f);
  EXPECT_TRUE(vec.GetField().IsOfType<TestFieldElement>());
  EXPECT_EQ(0, vec.Size());

  vec = FieldElementVector::MakeUninitialized(f, 5);
  EXPECT_TRUE(vec.GetField().IsOfType<TestFieldElement>());
  EXPECT_EQ(5U, vec.Size());
}

TEST(FieldElementVector, Set) {
  FieldElementVector vec = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(10), TestFieldElement::FromUint(11),
       TestFieldElement::FromUint(12), TestFieldElement::FromUint(13)});
  vec.Set(0, FieldElement(TestFieldElement::FromUint(20)));
  vec.Set(3, FieldElement(TestFieldElement::FromUint(30)));
  EXPECT_THAT(
      vec.As<TestFieldElement>(),
      ElementsAre(
          TestFieldElement::FromUint(20), TestFieldElement::FromUint(11),
          TestFieldElement::FromUint(12), TestFieldElement::FromUint(30)));
}

TEST(FieldElementVector, LinearCombination) {
  const FieldElementVector vec1 = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(10), TestFieldElement::FromUint(11),
       TestFieldElement::FromUint(12), TestFieldElement::FromUint(13)});
  const FieldElementVector vec2 = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(3), TestFieldElement::FromUint(2), TestFieldElement::FromUint(1),
       TestFieldElement::FromUint(0)});
  const FieldElementVector vec3 = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(10), TestFieldElement::FromUint(20),
       TestFieldElement::FromUint(30), TestFieldElement::FromUint(40)});
  const FieldElementVector coeffs = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(1), TestFieldElement::FromUint(2),
       TestFieldElement::FromUint(3)});
  FieldElementVector output = FieldElementVector::MakeUninitialized<TestFieldElement>(4);
  FieldElementVector::LinearCombination(
      coeffs, std::vector<ConstFieldElementSpan>({vec1.AsSpan(), vec2.AsSpan(), vec3.AsSpan()}),
      output);
  EXPECT_THAT(
      output.As<TestFieldElement>(),
      ElementsAre(
          TestFieldElement::FromUint(46), TestFieldElement::FromUint(75),
          TestFieldElement::FromUint(104), TestFieldElement::FromUint(133)));
}

TEST(FieldElementVector, LinearCombinationErrors) {
  const FieldElementVector vec1 = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(10), TestFieldElement::FromUint(11),
       TestFieldElement::FromUint(12), TestFieldElement::FromUint(13)});
  const FieldElementVector vec2 = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(10), TestFieldElement::FromUint(20)});
  const FieldElementVector vec3 = FieldElementVector::Make<LongFieldElement>(
      {LongFieldElement::FromUint(10), LongFieldElement::FromUint(20),
       LongFieldElement::FromUint(30), LongFieldElement::FromUint(40)});
  const FieldElementVector coeffs = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(1), TestFieldElement::FromUint(2)});
  FieldElementVector output = FieldElementVector::MakeUninitialized<TestFieldElement>(4);
  FieldElementVector output1 = FieldElementVector::MakeUninitialized<TestFieldElement>(3);
  FieldElementVector output2 = FieldElementVector::MakeUninitialized<LongFieldElement>(4);
  EXPECT_ASSERT(
      FieldElementVector::LinearCombination(
          coeffs, std::vector<ConstFieldElementSpan>({vec1.AsSpan()}), output),
      HasSubstr("Number of coefficients should match number of vectors."));
  EXPECT_ASSERT(
      FieldElementVector::LinearCombination(
          FieldElementVector::Make<TestFieldElement>(), std::vector<ConstFieldElementSpan>({}),
          output),
      HasSubstr("Linear combination of empty list is not defined."));
  EXPECT_ASSERT(
      FieldElementVector::LinearCombination(
          coeffs, std::vector<ConstFieldElementSpan>({vec1.AsSpan(), vec2.AsSpan()}), output),
      HasSubstr("Vectors must have same dimension."));
  EXPECT_ASSERT(
      FieldElementVector::LinearCombination(
          coeffs, std::vector<ConstFieldElementSpan>({vec1.AsSpan(), vec3.AsSpan()}), output),
      HasSubstr("Vectors must be over same field."));
  EXPECT_ASSERT(
      FieldElementVector::LinearCombination(
          coeffs, std::vector<ConstFieldElementSpan>({vec1.AsSpan(), vec1.AsSpan()}), output1),
      HasSubstr("Output must be same dimension as input."));
  EXPECT_ASSERT(
      FieldElementVector::LinearCombination(
          coeffs, std::vector<ConstFieldElementSpan>({vec1.AsSpan(), vec1.AsSpan()}), output2),
      HasSubstr("Output must be over same field as input."));
}

}  // namespace
}  // namespace starkware
