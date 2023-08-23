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

#include "starkware/crypt_tools/masked_hash.h"

#include <cstddef>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/big_int.h"
#include "starkware/crypt_tools/blake2s.h"
#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/utils/serialization.h"

namespace starkware {

namespace {

template <typename HashT>
HashT AsMaskedDigest(const BigInt<4>& val) {
  std::array<std::byte, BigInt<4>::SizeInBytes()> bytes{};
  BigInt<4> mask = 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_Z;
  mask >>= 8 * (32 - HashT::kNumEffectiveBytes);
  mask <<= 8 * (32 - HashT::kNumEffectiveBytes);
  Serialize(val & mask, bytes, /*use_big_endian=*/true);
  return HashT::InitDigestTo(bytes);
}

std::vector<std::byte> GenerateTestVector(size_t length) {
  std::vector<std::byte> test_vector;
  test_vector.reserve(length);

  char val = 0x11;
  for (size_t i = 0; i < length; i++) {
    test_vector.push_back(std::byte(val));
    val += 0x11;
    if (val == static_cast<char>(0x99)) {
      val = 0x11;
    }
  }

  return test_vector;
}

template <typename HashDigest>
class MaskedHashTest : public ::testing::Test {
 public:
};

using HashTypes =
    ::testing::Types<MaskedHash<Keccak256, 20, true>, MaskedHash<Keccak256, 32, true>>;
TYPED_TEST_CASE(MaskedHashTest, HashTypes);

TYPED_TEST(MaskedHashTest, EmptyString) {
  std::array<std::byte, 0> in{};

  EXPECT_EQ(
      AsMaskedDigest<TypeParam>(
          0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470_Z),
      TypeParam::HashBytesWithLength(in));
}

TYPED_TEST(MaskedHashTest, TestingString) {
  constexpr std::array<std::byte, 7> kTesting = MakeByteArray<'t', 'e', 's', 't', 'i', 'n', 'g'>();

  EXPECT_EQ(
      AsMaskedDigest<TypeParam>(
          0x5f16f4c7f149ac4f9510d9cf8cf384038ad348b3bcdc01915f95de12df9d1b02_Z),
      TypeParam::HashBytesWithLength(kTesting));
}

TYPED_TEST(MaskedHashTest, HashTwoHashesWithLength) {
  constexpr std::array<std::byte, 7> kTesting = MakeByteArray<'t', 'e', 's', 't', 'i', 'n', 'g'>();

  TypeParam h1 = TypeParam::HashBytesWithLength(kTesting);
  std::array<std::byte, 2 * TypeParam::kDigestNumBytes> buf{};
  std::copy_n(h1.GetDigest().begin(), TypeParam::kDigestNumBytes, buf.data());
  std::copy_n(
      h1.GetDigest().begin(), TypeParam::kDigestNumBytes, buf.data() + TypeParam::kDigestNumBytes);

  // Check that HashBytesWithLength(h1||h1) == TypeParam::Hash(h1, h1).
  EXPECT_EQ(TypeParam::HashBytesWithLength(buf), TypeParam::Hash(h1, h1));
}

TYPED_TEST(MaskedHashTest, TestVectors) {
  EXPECT_EQ(
      AsMaskedDigest<TypeParam>(
          0x579bb7619d88a618d64f8a31990ceed4f416b3bfc4a58dc4d253fd5260316bf9_Z),
      TypeParam::HashBytesWithLength(GenerateTestVector(32)));

  EXPECT_EQ(
      AsMaskedDigest<TypeParam>(
          0x01fc6a3d48aaee52c507048e1cd1891a0082098a992b825229b50f00d6993148_Z),
      TypeParam::HashBytesWithLength(GenerateTestVector(135)));

  EXPECT_EQ(
      AsMaskedDigest<TypeParam>(
          0x1e12a9123f39d457468eccb5ab485128b5294c5eb0c83a1cf497b998ab3d5f66_Z),
      TypeParam::HashBytesWithLength(GenerateTestVector(999)));

  EXPECT_EQ(
      AsMaskedDigest<TypeParam>(
          0x59ae1d570a3a4bf6ceabeb13a3b37dcedd2d01ac49b365527922514091e7c7c4_Z),
      TypeParam::HashBytesWithLength(GenerateTestVector(1000)));
}

TYPED_TEST(MaskedHashTest, HashName) {
  using HashMsb = MaskedHash<Keccak256, 20, true>;
  EXPECT_EQ(HashMsb::HashName(), "keccak256_masked160_msb");
  using HashLsb = MaskedHash<Keccak256, 12, false>;
  EXPECT_EQ(HashLsb::HashName(), "keccak256_masked96_lsb");
}

TEST(MaskedHashTest, MaskedMsbAndMaskedLsb) {
  const std::string str = "0x4493758de1393a3bbf47cb8fcc99ca141a82548bd17eb54be45e8946fbc9441d";
  std::array<std::byte, Blake2s256::kDigestNumBytes> str_as_bytes{};
  HexStringToBytes(str, str_as_bytes);

  // Blake256 (no mask).
  const Blake2s256 hash = Blake2s256::HashBytesWithLength(str_as_bytes);
  EXPECT_EQ(hash.ToString(), "0xdcba38e4aea16a5f87e8b76039958941cc2b19c1bd836b3a53506489395aa54d");

  // Masked Blake MSB.
  using HashMsb = MaskedHash<Blake2s256, 20, true>;
  const HashMsb hash_msb = HashMsb::HashBytesWithLength(str_as_bytes);
  EXPECT_EQ(
      hash_msb.ToString(), "0xdcba38e4aea16a5f87e8b76039958941cc2b19c1000000000000000000000000");

  // Masked Blake LSB.
  using HashLsb = MaskedHash<Blake2s256, 18, false>;
  const HashLsb hash_lsb = HashLsb::HashBytesWithLength(str_as_bytes);
  EXPECT_EQ(
      hash_lsb.ToString(), "0x00000000000000000000000000008941cc2b19c1bd836b3a53506489395aa54d");
}

}  // namespace

}  // namespace starkware
