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

#include "starkware/algebra/polymorphic/field_element_span.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::ElementsAre;

TEST(FieldElementSpan, BasicTest) {
  std::vector<TestFieldElement> vec = {TestFieldElement::FromUint(1), TestFieldElement::FromUint(3),
                                       TestFieldElement::FromUint(5)};
  gsl::span<TestFieldElement> vec_span(vec);
  FieldElementSpan polymorphic_span(vec_span);

  EXPECT_TRUE(polymorphic_span.GetField().IsOfType<TestFieldElement>());

  EXPECT_EQ(3U, polymorphic_span.Size());
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(1)), polymorphic_span[0]);
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(3)), polymorphic_span[1]);
  gsl::span<TestFieldElement> span2 = polymorphic_span.As<TestFieldElement>();
  EXPECT_EQ(vec_span, span2);
}

TEST(FieldElementSpan, FromFieldElementVector) {
  FieldElementVector vec = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)});
  FieldElementSpan span(vec);

  EXPECT_EQ(2U, span.Size());
  EXPECT_THAT(
      span.As<TestFieldElement>(),
      ElementsAre(TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)));
}

TEST(FieldElementSpan, GetField) {
  FieldElementVector vec = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)});
  FieldElementSpan span(vec);

  EXPECT_TRUE(span.GetField().IsOfType<TestFieldElement>());
}

TEST(FieldElementSpan, Eq) {
  std::vector<TestFieldElement> vec1 = {TestFieldElement::FromUint(4),
                                        TestFieldElement::FromUint(6)};
  std::vector<TestFieldElement> vec2 = {TestFieldElement::FromUint(4),
                                        TestFieldElement::FromUint(6)};
  std::vector<TestFieldElement> vec3 = {TestFieldElement::FromUint(4),
                                        TestFieldElement::FromUint(20)};

  FieldElementSpan span1(gsl::make_span(vec1));
  FieldElementSpan span2(gsl::make_span(vec2));
  FieldElementSpan span3(gsl::make_span(vec3));

  EXPECT_TRUE(span1 == span2);
  EXPECT_FALSE(span1 != span2);
  EXPECT_FALSE(span1 == span3);
  EXPECT_TRUE(span1 != span3);
}

TEST(FieldElementSpan, Assignment) {
  std::vector<TestFieldElement> vec = {TestFieldElement::FromUint(4),
                                       TestFieldElement::FromUint(6)};
  FieldElementSpan span(gsl::make_span(vec));

  span.Set(0, span[1] + span[1]);

  EXPECT_EQ(vec[0], TestFieldElement::FromUint(12));
  EXPECT_EQ(vec[1], TestFieldElement::FromUint(6));
}

TEST(FieldElementSpan, CopyConstructor) {
  std::vector<TestFieldElement> vec = {TestFieldElement::FromUint(4),
                                       TestFieldElement::FromUint(6)};
  FieldElementSpan span1(gsl::make_span(vec));
  FieldElementSpan span2(span1);  // NOLINT: clang-tidy warns that local copy is never modified.

  EXPECT_TRUE(span1 == span2);
}

TEST(FieldElementSpan, AssignmentOperator) {
  std::vector<TestFieldElement> vec1 = {TestFieldElement::FromUint(4),
                                        TestFieldElement::FromUint(6)};
  std::vector<TestFieldElement> vec2;
  FieldElementSpan span1(gsl::make_span(vec1));
  FieldElementSpan span2(gsl::make_span(vec2));

  EXPECT_FALSE(span1 == span2);
  span2 = span1;
  EXPECT_TRUE(span1 == span2);

  // Self assignment.
  span2.operator=(span2);  // Call operator= explicitly to avoid self assignment warning.
  EXPECT_TRUE(span1 == span2);
}

TEST(FieldElementSpan, SubSpan) {
  using testing::ElementsAreArray;
  std::vector<TestFieldElement> vec = {
      TestFieldElement::FromUint(4), TestFieldElement::FromUint(6), TestFieldElement::FromUint(17),
      TestFieldElement::FromUint(12), TestFieldElement::FromUint(18)};

  FieldElementSpan span(gsl::make_span(vec));

  EXPECT_THAT(
      span.SubSpan(2).As<TestFieldElement>(), ElementsAreArray(gsl::make_span(vec).subspan(2)));
  EXPECT_THAT(
      span.SubSpan(3, 2).As<TestFieldElement>(),
      ElementsAreArray(gsl::make_span(vec).subspan(3, 2)));
}

TEST(FieldElementSpan, Copy) {
  FieldElementVector src_vec = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)});
  ConstFieldElementSpan src_span(src_vec);
  FieldElementVector dest_vec = FieldElementVector::MakeUninitialized<TestFieldElement>(2);
  FieldElementSpan dest_span(dest_vec);
  dest_span.CopyDataFrom(src_span);
  EXPECT_THAT(
      dest_span.As<TestFieldElement>(),
      ElementsAre(TestFieldElement::FromUint(4), TestFieldElement::FromUint(6)));

  FieldElementVector dest_vec_big = FieldElementVector::MakeUninitialized<TestFieldElement>(3);
  FieldElementSpan dest_span_big(dest_vec_big);
  EXPECT_ASSERT(dest_span_big.CopyDataFrom(src_span), testing::HasSubstr("different size"));

  FieldElementVector dest_vec_small = FieldElementVector::MakeUninitialized<TestFieldElement>(1);
  FieldElementSpan dest_span_small(dest_vec_small);
  EXPECT_ASSERT(dest_span_small.CopyDataFrom(src_span), testing::HasSubstr("different size"));
}

TEST(ConstFieldElementSpan, BasicTest) {
  std::vector<TestFieldElement> vec = {
      TestFieldElement::FromUint(4), TestFieldElement::FromUint(6), TestFieldElement::FromUint(17),
      TestFieldElement::FromUint(12), TestFieldElement::FromUint(18)};
  ConstFieldElementSpan polymorphic_span{gsl::span<const TestFieldElement>(vec)};
  EXPECT_TRUE(polymorphic_span.GetField().IsOfType<TestFieldElement>());
  EXPECT_EQ(5U, polymorphic_span.Size());
  EXPECT_EQ(FieldElement(TestFieldElement::FromUint(4)), polymorphic_span[0]);
  EXPECT_EQ(gsl::make_span(vec), polymorphic_span.As<TestFieldElement>());
  EXPECT_THAT(
      polymorphic_span.SubSpan(1, 1).As<TestFieldElement>(),
      ElementsAre(TestFieldElement::FromUint(6)));
}

TEST(ConstFieldElementSpan, UniqueVectorsToSpans) {
  std::vector<FieldElementVector> vec;
  vec.push_back(FieldElementVector::MakeUninitialized<TestFieldElement>(2));
  vec.push_back(FieldElementVector::MakeUninitialized<TestFieldElement>(3));

  std::vector<ConstFieldElementSpan> spans(vec.begin(), vec.end());
  EXPECT_EQ(2U, spans[0].Size());
  EXPECT_EQ(3U, spans[1].Size());
}

}  // namespace
}  // namespace starkware
