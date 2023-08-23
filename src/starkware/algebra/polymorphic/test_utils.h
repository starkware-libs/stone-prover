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

#ifndef STARKWARE_ALGEBRA_POLYMORPHIC_TEST_UTILS_H_
#define STARKWARE_ALGEBRA_POLYMORPHIC_TEST_UTILS_H_

#include <utility>

#include "gmock/gmock.h"

#include "starkware/algebra/polymorphic/field_element_vector.h"

namespace starkware {

namespace algebra {
namespace details {

/*
  IsFieldElementVector implementation done in two classes, like other matchers in GMock. The reason
  is that when the matcher instance is created, the type it is supposed to match is unknown. When it
  is known, GMock expects a class that implements testing::MatcherInterface<T> for that type.
*/
template <typename FieldElementT, typename Container, typename T>
class IsFieldElementVectorMatcherImpl : public testing::MatcherInterface<T> {
 public:
  explicit IsFieldElementVectorMatcherImpl(testing::Matcher<Container> matcher)
      : matcher_(std::move(matcher)) {}
  void DescribeTo(::std::ostream* os) const override { matcher_.DescribeTo(os); }
  void DescribeNegationTo(::std::ostream* os) const override { matcher_.DescribeNegationTo(os); }
  bool MatchAndExplain(T x, testing::MatchResultListener* listener) const override {
    return matcher_.MatchAndExplain(x.template As<FieldElementT>(), listener);
  }

 private:
  const testing::Matcher<Container> matcher_;
};

template <typename FieldElementT, typename Matcher>
class IsFieldElementVectorMatcher {
 public:
  explicit IsFieldElementVectorMatcher(const Matcher& matcher) : matcher_(matcher) {}

  template <typename T>
  operator ::testing::Matcher<T>() const {  // NOLINT: clang-tidy expect explicit. GMock does not.
    using Container = typename std::result_of<decltype (
        &std::remove_reference<T>::type::template As<FieldElementT>)(T)>::type;
    return ::testing::Matcher<T>(new IsFieldElementVectorMatcherImpl<FieldElementT, Container, T>(
        testing::SafeMatcherCast<Container>(matcher_)));
  }

 private:
  const Matcher matcher_;
};

}  // namespace details
}  // namespace algebra

/*
  A matcher that acts on a FieldElementVector and checks that the inner std::vector matches to
  inner_matcher.
  Example usage:
    FieldElementVector v;
    ...
    EXPECT_THAT(v, IsFieldElementVector<FieldElementT>(ElementsAreArray(...)));
*/
template <typename FieldElementT, typename Matcher>
auto IsFieldElementVector(const Matcher& matcher) {
  return algebra::details::IsFieldElementVectorMatcher<FieldElementT, Matcher>(matcher);
}

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_POLYMORPHIC_TEST_UTILS_H_
