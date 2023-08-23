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

#include "starkware/crypt_tools/keccak_256.h"

#include <cstddef>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/big_int.h"
#include "starkware/crypt_tools/test_utils.h"
#include "starkware/utils/serialization.h"

namespace starkware {

namespace {

Keccak256 AsDigest(const BigInt<4>& val) {
  std::array<std::byte, BigInt<4>::SizeInBytes()> bytes{};
  Serialize(val, bytes, /*use_big_endian=*/true);

  return Keccak256::InitDigestTo(bytes);
}

TEST(Keccak256, EmptyString) {
  std::array<std::byte, 0> in{};

  EXPECT_EQ(
      AsDigest(0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470_Z),
      Keccak256::HashBytesWithLength(in));
}

TEST(Keccak256, TestingString) {
  constexpr std::array<std::byte, 7> kTesting = MakeByteArray<'t', 'e', 's', 't', 'i', 'n', 'g'>();

  EXPECT_EQ(
      AsDigest(0x5f16f4c7f149ac4f9510d9cf8cf384038ad348b3bcdc01915f95de12df9d1b02_Z),
      Keccak256::HashBytesWithLength(kTesting));
}

TEST(Keccak256, HashTwoHashesWithLength) {
  constexpr std::array<std::byte, 7> kTesting = MakeByteArray<'t', 'e', 's', 't', 'i', 'n', 'g'>();

  Keccak256 h1 = Keccak256::HashBytesWithLength(kTesting);
  std::array<std::byte, 2 * Keccak256::kDigestNumBytes> buf{};
  std::copy_n(h1.GetDigest().begin(), Keccak256::kDigestNumBytes, buf.data());
  std::copy_n(
      h1.GetDigest().begin(), Keccak256::kDigestNumBytes, buf.data() + Keccak256::kDigestNumBytes);

  // Check that HashBytesWithLength(h1||h1) == Keccak256::Hash(h1, h1).
  EXPECT_EQ(Keccak256::HashBytesWithLength(buf), Keccak256::Hash(h1, h1));
}

TEST(Keccak256, TestVectors) {
  EXPECT_EQ(
      AsDigest(0x579bb7619d88a618d64f8a31990ceed4f416b3bfc4a58dc4d253fd5260316bf9_Z),
      Keccak256::HashBytesWithLength(GenerateTestVector(32)));

  EXPECT_EQ(
      AsDigest(0x01fc6a3d48aaee52c507048e1cd1891a0082098a992b825229b50f00d6993148_Z),
      Keccak256::HashBytesWithLength(GenerateTestVector(135)));

  EXPECT_EQ(
      AsDigest(0x1e12a9123f39d457468eccb5ab485128b5294c5eb0c83a1cf497b998ab3d5f66_Z),
      Keccak256::HashBytesWithLength(GenerateTestVector(999)));

  EXPECT_EQ(
      AsDigest(0x59ae1d570a3a4bf6ceabeb13a3b37dcedd2d01ac49b365527922514091e7c7c4_Z),
      Keccak256::HashBytesWithLength(GenerateTestVector(1000)));
}

}  // namespace

}  // namespace starkware
