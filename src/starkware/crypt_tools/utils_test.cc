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

#include "starkware/crypt_tools/utils.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "gtest/gtest.h"

#include "starkware/crypt_tools/blake2s.h"
#include "starkware/randomness/prng.h"

namespace starkware {

template <typename T>
class HashUtilsTest : public ::testing::Test {};

using TestedHashTypes = ::testing::Types<Blake2s256>;

TYPED_TEST_CASE(HashUtilsTest, TestedHashTypes);

TYPED_TEST(HashUtilsTest, HashBytesWithLength) {
  Prng prng;
  std::array<std::byte, 320> bytes{};
  prng.GetRandomBytes(bytes);

  const TypeParam hash1 = TypeParam::HashBytesWithLength(bytes);
  const TypeParam hash2 = TypeParam::HashBytesWithLength(bytes);

  EXPECT_EQ(hash1, hash2);

  std::array<std::byte, 640> double_bytes{};
  std::copy(bytes.begin(), bytes.end(), double_bytes.begin());
  std::copy(bytes.begin(), bytes.end(), double_bytes.begin() + 320);

  const TypeParam hash3 = TypeParam::HashBytesWithLength(bytes, hash1);
  const TypeParam hash4 = TypeParam::HashBytesWithLength(double_bytes);

  EXPECT_NE(hash3, hash4);
}

TYPED_TEST(HashUtilsTest, InitDigestFromSpan) {
  Prng prng;
  TypeParam hash = prng.RandomHash<TypeParam>();
  std::array<std::byte, TypeParam::kDigestNumBytes> bytes{
      InitDigestFromSpan<TypeParam>(hash.GetDigest())};
  EXPECT_EQ(bytes, hash.GetDigest());
}

}  // namespace starkware
