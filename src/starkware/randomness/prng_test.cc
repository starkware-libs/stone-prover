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

#include "starkware/randomness/prng.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "gtest/gtest.h"

#include "starkware/algebra/big_int.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/hash_chain.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {
namespace {

using testing::HasSubstr;

template <typename T>
class PrngTest : public ::testing::Test {};

using IntTypes = ::testing::Types<uint16_t, uint32_t, uint64_t>;
TYPED_TEST_CASE(PrngTest, IntTypes);

template <typename T>
class PrngFieldTest : public ::testing::Test {};
using FieldTypes = ::testing::Types<TestFieldElement>;
TYPED_TEST_CASE(PrngFieldTest, FieldTypes);

TYPED_TEST(PrngTest, TwoInvocationsAreNotIdentical) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  auto a = prng.UniformInt<TypeParam>(0, std::numeric_limits<TypeParam>::max());
  auto b = prng.UniformInt<TypeParam>(0, std::numeric_limits<TypeParam>::max());
  EXPECT_NE(a, b);
}

TYPED_TEST(PrngTest, VectorInvocation) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  auto v = prng.UniformIntVector<TypeParam>(0, std::numeric_limits<TypeParam>::max(), 10);
  auto w = prng.UniformIntVector<TypeParam>(0, std::numeric_limits<TypeParam>::max(), 10);

  ASSERT_EQ(v.size(), 10U);
  ASSERT_EQ(w.size(), 10U);
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_NE(v.at(i), w.at(i));
  }
}

TEST(PrngTest, BoolVector) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  const size_t size = 1000;
  auto v = prng.UniformBoolVector(size);
  auto w = prng.UniformBoolVector(size);

  EXPECT_EQ(v.size(), size);
  EXPECT_EQ(w.size(), size);
  EXPECT_NE(v, w);

  EXPECT_NEAR(std::count(v.begin(), v.end(), false), size / 2.0, size / 10.0);
  EXPECT_NEAR(std::count(w.begin(), w.end(), false), size / 2.0, size / 10.0);
}

/*
  1. Reseed with some seed
  2. Generate a list of numbers
  3. Reseed with the same seed and check that a second sequence of numbers is identical to the
     saved one.
*/
TYPED_TEST(PrngTest, ReseedingWithSameSeedYieldsSameRandomness) {
  Prng prng;
  unsigned size = 100;
  std::array<std::byte, 5> seed = MakeByteArray<1, 2, 3, 4, 5>();
  prng.Reseed(seed);
  std::vector<TypeParam> vals(size);
  std::transform(vals.begin(), vals.end(), vals.begin(), [&prng](const TypeParam& /* unused */) {
    return prng.UniformInt<TypeParam>(0, std::numeric_limits<TypeParam>::max());
  });
  prng.Reseed(seed);
  for (auto& val : vals) {
    auto new_val = prng.UniformInt<TypeParam>(0, std::numeric_limits<TypeParam>::max());
    ASSERT_EQ(val, new_val);
  }
}

TEST(PrngTest, Clone) {
  const unsigned size = 100;
  Prng prng1;
  auto prng2 = prng1.Clone();
  const std::vector<std::byte> vec1 = prng1.RandomByteVector(size);
  const std::vector<std::byte> vec2 = prng2->RandomByteVector(size);
  EXPECT_EQ(vec1, vec2);

  auto prng3 = prng1.Clone();
  const std::vector<std::byte> vec3 = prng3->RandomByteVector(size);
  const std::vector<std::byte> vec4 = prng1.RandomByteVector(size);
  EXPECT_EQ(vec3, vec4);
}

TEST(PrngTest, CloneExplicitSeed) {
  const unsigned size = 100;
  Prng prng1(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  auto prng2 = prng1.Clone();
  const std::vector<std::byte> vec1 = prng1.RandomByteVector(size);
  const std::vector<std::byte> vec2 = prng2->RandomByteVector(size);
  EXPECT_EQ(vec1, vec2);

  auto prng3 = prng1.Clone();
  const std::vector<std::byte> vec3 = prng3->RandomByteVector(size);
  const std::vector<std::byte> vec4 = prng1.RandomByteVector(size);
  EXPECT_EQ(vec3, vec4);
}

TEST(PrngTest, MoveConstructor) {
  const unsigned size = 100;
  Prng prng1(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  Prng prng2(Prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>()));
  const std::vector<std::byte> vec1 = prng1.RandomByteVector(size);
  const std::vector<std::byte> vec2 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec1, vec2);

  Prng prng3(std::move(prng1));
  const std::vector<std::byte> vec3 = prng3.RandomByteVector(size);
  const std::vector<std::byte> vec4 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec3, vec4);
}

TEST(PrngTest, MoveAssignment) {
  const unsigned size = 100;
  Prng prng1(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  Prng prng2;
  prng2 = Prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  const std::vector<std::byte> vec1 = prng1.RandomByteVector(size);
  const std::vector<std::byte> vec2 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec1, vec2);

  Prng prng3;
  prng3 = std::move(prng1);
  const std::vector<std::byte> vec3 = prng3.RandomByteVector(size);
  const std::vector<std::byte> vec4 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec3, vec4);

  Prng prng5;
  const std::vector<std::byte> vec_different = prng5.RandomByteVector(size);
  prng5 = std::move(prng3);
  const std::vector<std::byte> vec5 = prng5.RandomByteVector(size);
  const std::vector<std::byte> vec6 = prng2.RandomByteVector(size);
  EXPECT_EQ(vec5, vec6);

  EXPECT_NE(vec1, vec_different);
  EXPECT_NE(vec3, vec_different);
  EXPECT_NE(vec5, vec_different);
}

TYPED_TEST(PrngFieldTest, FieldElementVectorInvocation) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());

  auto v = prng.RandomFieldElementVector<TypeParam>(10);
  auto w = prng.RandomFieldElementVector<TypeParam>(7);
  ASSERT_EQ(v.size(), 10U);
  ASSERT_EQ(w.size(), 7U);
  for (size_t i = 0; i < 7; ++i) {
    EXPECT_NE(v.at(i), w.at(i));
  }
}

TYPED_TEST(PrngFieldTest, VectorSingletonOrEmpty) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());

  ASSERT_EQ(0U, prng.RandomFieldElementVector<TypeParam>(0).size());
  ASSERT_EQ(1U, prng.RandomFieldElementVector<TypeParam>(1).size());
}

/*
  Test case: n_elements > (max - min + 1) / 2
  Expects: exception.
*/
TEST(PrngTest, UniformDistinctIntVectorAssert) {
  Prng prng;
  EXPECT_ASSERT(prng.UniformDistinctIntVector<uint16_t>(0, 10, 6), HasSubstr("Number of elements"));
  EXPECT_ASSERT(
      prng.UniformDistinctIntVector<uint16_t>(
          std::numeric_limits<uint16_t>::min(), std::numeric_limits<uint16_t>::max(),
          (std::numeric_limits<uint16_t>::max() - std::numeric_limits<uint16_t>::min()) / 2 + 2),
      HasSubstr("Number of elements"));
}

/*
  Test case: n_elements = 0.
  Expects: vector of size 0.
  Test case: n_elements = 1.
  Expects: vector of size 1.
*/
TYPED_TEST(PrngTest, UniformDistinctIntVectorSmall) {
  Prng prng;
  std::vector<TypeParam> size_zero_vec = prng.UniformDistinctIntVector<TypeParam>(
      std::numeric_limits<TypeParam>::min(), std::numeric_limits<TypeParam>::max(), 0);
  ASSERT_EQ(size_zero_vec.size(), 0);
  std::vector<TypeParam> size_one_vec = prng.UniformDistinctIntVector<TypeParam>(
      std::numeric_limits<TypeParam>::min(), std::numeric_limits<TypeParam>::max(), 1);
  ASSERT_EQ(size_one_vec.size(), 1);
}

/*
  Test case: creates 50 times a uniform distinct int vector of size 10, sorts the vector and uses
  std::uniqe to check if there are repetitions. Expects: the original vector and the resized vector
  are equal.
*/
TYPED_TEST(PrngTest, UniformDistinctIntVectorUnique) {
  Prng prng;
  for (unsigned i = 0; i < 50; ++i) {
    std::vector<TypeParam> vec = prng.UniformDistinctIntVector<TypeParam>(0, 30, 10);
    std::sort(vec.begin(), vec.end());
    std::vector<TypeParam> unique_vec = vec;
    auto it = std::unique(unique_vec.begin(), unique_vec.end());
    unique_vec.resize(std::distance(unique_vec.begin(), it));
    ASSERT_TRUE(vec.size() == unique_vec.size());
  }
}

TEST(PrngTest, UniformBigInt) {
  Prng prng;
  using ValueType = BigInt<4>;
  const size_t sqrt_n_tries = 16;
  const size_t n_tries = sqrt_n_tries * sqrt_n_tries;

  ValueType range_a = ValueType::Zero();
  ValueType range_b = -ValueType::One();
  ValueType mid = range_b;
  mid >>= 1;

  size_t count = 0;

  EXPECT_EQ(prng.UniformBigInt(range_a, range_a), range_a);
  EXPECT_EQ(prng.UniformBigInt(range_b, range_b), range_b);

  for (size_t i = 0; i < n_tries; ++i) {
    if (prng.UniformBigInt(range_a, range_b) > mid) {
      count++;
    }
  }

  // 5 sigmas.
  EXPECT_NEAR(count, (float)n_tries / 2, 5 * (float)sqrt_n_tries / 2);
}

}  // namespace
}  // namespace starkware
