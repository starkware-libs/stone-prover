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

#include "starkware/stl_utils/containers.h"

#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::ElementsAre;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

TEST(StlContainers, GetMapKeys) {
  std::map<int, int> m;
  m[0] = 5;
  m[6] = 9;
  m[3] = 7;
  EXPECT_THAT(Keys(m), UnorderedElementsAre(0, 6, 3));
}

TEST(StlContainers, Count) {
  std::vector<int> v = {1, -1, 2, 3, 2, 1, 1};
  EXPECT_EQ(Count(v, 1), 3U);
  EXPECT_EQ(Count(v, -1), 1U);
  EXPECT_EQ(Count(v, 2), 2U);
  EXPECT_EQ(Count(v, 3), 1U);
}

TEST(StlContainers, Sum) {
  EXPECT_EQ(Sum(std::vector<int>{1, -1, 2, 3, 2, 1, 1}), 9U);
  EXPECT_EQ(Sum(std::vector<int>{}), 0U);
  EXPECT_EQ(Sum(std::vector<int>{5}), 5U);

  EXPECT_EQ(Sum(std::vector<int>{5}, 0), 5U);
  EXPECT_EQ(Sum(std::vector<int>{5}, 3), 8U);
  EXPECT_EQ(Sum(std::vector<int>{}, 0), 0U);
  EXPECT_EQ(Sum(std::vector<int>{}, 3), 3U);
}

TEST(StlContainers, HasKey) {
  std::map<int, int> m;
  m[0] = 5;
  m[6] = 9;
  m[3] = 7;
  EXPECT_TRUE(HasKey(m, 0));
  EXPECT_FALSE(HasKey(m, 1));
  EXPECT_FALSE(HasKey(m, 2));
  EXPECT_TRUE(HasKey(m, 3));
  EXPECT_FALSE(HasKey(m, 4));
  EXPECT_FALSE(HasKey(m, 5));
  EXPECT_TRUE(HasKey(m, 6));
}

TEST(StlContainers, SetUnion) {
  std::set<int> set1 = {4, 7, 100};
  std::set<int> set2 = {50, 100, 150};
  EXPECT_THAT(SetUnion(set1, set2), UnorderedElementsAre(4, 7, 50, 100, 150));
  EXPECT_THAT(SetUnion(set1, {}), UnorderedElementsAre(4, 7, 100));
  EXPECT_THAT(SetUnion<int>({}, {}), IsEmpty());
}

TEST(StlContainers, AreDisjoint) {
  EXPECT_FALSE(AreDisjoint(std::set<int>{1}, std::set<int>{1}));
  EXPECT_TRUE(AreDisjoint(std::set<unsigned>{1}, std::set<unsigned>{7}));
  EXPECT_FALSE(AreDisjoint(std::set<float>{1}, std::set<float>{2, 1}));
  EXPECT_TRUE(AreDisjoint(std::set<int>{1, 2}, std::set<int>{7, 9}));
  EXPECT_FALSE(AreDisjoint(std::set<int>{19, 17, 0}, std::set<int>{2, 1, 11, 23, 19}));
  EXPECT_TRUE(AreDisjoint(std::set<int>{8, 7, 6, 5}, std::set<int>{3}));
}

TEST(StlContainers, HasDuplicates) {
  EXPECT_FALSE(HasDuplicates<int>(std::vector<int>{}));
  EXPECT_FALSE(HasDuplicates<int>(std::vector<int>{1, 10, 5, 3}));
  EXPECT_TRUE(HasDuplicates<int>(std::vector<int>{1, 10, 5, 3, 10, 7}));
  EXPECT_TRUE(HasDuplicates<int>(std::vector<int>{1, 1, 2, 2, 3, 3}));
  EXPECT_TRUE(HasDuplicates<int>(std::vector<int>{1, 10, 5, 3, 7, 1}));
}

/*
  Test the "<<" operator for an STL set. Test cases are: empty set, singleton, and a 4-element set
  constructed from scratch.
*/
TEST(StlContainer, SetToStream) {
  std::set<unsigned> some_set;
  // Empty set.
  std::stringstream some_stream;
  some_stream << some_set;
  EXPECT_EQ("{}", some_stream.str());

  // Singleton.
  some_stream.clear();
  some_stream.str(std::string());
  some_set.insert(314);
  some_stream << some_set;
  EXPECT_EQ("{314}", some_stream.str());

  // 4-element set from scratch.
  some_set = {0, 1, 2, 3};
  some_stream.clear();
  some_stream.str(std::string());
  some_stream << some_set;
  EXPECT_EQ("{0, 1, 2, 3}", some_stream.str());
}

/*
  Test the "<<" operator for an STL vector. Test cases are: empty set, singleton, and a 4-element
  set constructed from scratch.
*/
TEST(StlContainer, VectorToStream) {
  std::vector<unsigned> v;
  // Empty set.
  std::stringstream s;
  s << v;
  EXPECT_EQ("[]", s.str());

  // Singleton.
  s.clear();
  s.str(std::string());
  v.push_back(314);
  s << v;
  EXPECT_EQ("[314]", s.str());

  // 4-element set from scratch.
  v = {0, 1, 2, 3};
  s.clear();
  s.str(std::string());
  s << v;
  EXPECT_EQ("[0, 1, 2, 3]", s.str());
}

/*
  Test the "<<" operator for an STL map. Test cases are: empty set, singleton, and a 4-element
  set constructed from scratch.
*/
TEST(StlContainer, MapToStream) {
  std::map<unsigned, std::string> m;
  // Empty set.
  std::stringstream s;
  s << m;
  EXPECT_EQ("{}", s.str());

  // Singleton.
  s.clear();
  s.str(std::string());
  m[314] = "Pi";
  s << m;
  EXPECT_EQ("{314: Pi}", s.str());

  // 4-element set from scratch.
  m.clear();
  s.clear();
  s.str(std::string());
  m[0] = "Yes";
  m[1] = "sir";
  m[2] = "I can";
  m[3] = "boogie!";
  s << m;
  EXPECT_EQ("{0: Yes, 1: sir, 2: I can, 3: boogie!}", s.str());
}

constexpr std::array<int, 1> ArrayWithVal(int val) {
  std::array<int, 1> arr{};
  constexpr_at(arr, 0) = val;
  return arr;
}

TEST(StlContainer, contexpr_at) {
  constexpr auto kArr = ArrayWithVal(1);
  constexpr int kOne = gsl::at(kArr, 0);
  static_assert(kOne == 1, "should be equal");
}

template <typename T>
void CheckSpan(gsl::span<const gsl::span<const T>> s) {
  EXPECT_THAT(s, ElementsAre(ElementsAre(1, 2, 3), ElementsAre(4, 5, 6)));
}

TEST(StlContainer, ConstSpanAdapter) {
  const std::vector<std::vector<int>> v{{1, 2, 3}, {4, 5, 6}};
  CheckSpan<int>(ConstSpanAdapter(v));
}

template <typename T>
void ModifySpan(gsl::span<const gsl::span<T>> s) {
  s[0][0] = 5;
  s[0][1] = 6;
  s[1][0] = 7;
  s[1][1] = 8;
}

TEST(StlContainer, SpanAdapter) {
  std::vector<std::vector<int>> v{{1, 2}, {3, 4}};
  ModifySpan<int>(SpanAdapter(v));
  EXPECT_THAT(v, ElementsAre(ElementsAre(5, 6), ElementsAre(7, 8)));
}

TEST(FftTest, UncheckedAt) {
  std::array<int, 1> arr{17};

  static_assert(
      std::is_same_v<decltype(UncheckedAt(gsl::span<int>(arr), 0)), int&>,
      "type of UncheckedAt(span, idx) should be FieldElementT&");
  static_assert(
      std::is_same_v<decltype(UncheckedAt(gsl::span<const int>(arr), 0)), const int&>,
      "type of UncheckedAt(const span, idx) should be const FieldElementT&");
  EXPECT_EQ(arr[0], UncheckedAt(gsl::make_span(arr), 0));
}

/*
  Test the consumption of an empty optional object fails.
*/
TEST(StlContainers, ConsumeOptionalEmpty) {
  std::optional<std::unique_ptr<int>> empty_optional;
  EXPECT_ASSERT(
      ConsumeOptional(std::move(empty_optional)),
      HasSubstr("The optional object doesn't have a value."));
}

/*
  Test the consumption of a nonempty optional object is done properly: the value is not allocated
  anew and the optional consumed is reset.
*/
TEST(StlContainers, ConsumeOptionalFull) {
  auto ptr = std::make_unique<int>(10);
  std::optional<std::unique_ptr<int>> full_optional = std::move(ptr);
  int* ptr_data = full_optional->get();
  EXPECT_EQ(ptr_data, ConsumeOptional(std::move(full_optional)).get());
  EXPECT_FALSE(full_optional.has_value());  // NOLINT: test value after move.
}

}  // namespace
}  // namespace starkware
