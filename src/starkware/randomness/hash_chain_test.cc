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

#include <map>
#include <utility>

#include "gtest/gtest.h"

#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/randomness/hash_chain.h"

namespace starkware {
namespace {

/*
  Tests constants.
*/
const size_t kKeccakId = typeid(Keccak256).hash_code();

std::vector<std::byte> random_bytes_1st_keccak256 = {
    std::byte{0x07}, std::byte{0x7C}, std::byte{0xE2}, std::byte{0x30},
    std::byte{0x83}, std::byte{0x44}, std::byte{0x67}, std::byte{0xE7}};

std::vector<std::byte> random_bytes_1000th_keccak256 = {
    std::byte{0xD1}, std::byte{0x74}, std::byte{0x78}, std::byte{0xD2},
    std::byte{0x31}, std::byte{0xC2}, std::byte{0xAF}, std::byte{0x63}};

std::vector<std::byte> random_bytes_1001st_keccak256 = {
    std::byte{0xA0}, std::byte{0xDA}, std::byte{0xBD}, std::byte{0x71},
    std::byte{0xEE}, std::byte{0xAB}, std::byte{0x82}, std::byte{0xAC}};

std::map<size_t, std::vector<std::byte>> random_bytes_keccak256 = {
    std::make_pair(1, random_bytes_1st_keccak256),
    std::make_pair(1000, random_bytes_1000th_keccak256),
    std::make_pair(1001, random_bytes_1001st_keccak256)};

const std::map<size_t, std::map<size_t, std::vector<std::byte>>> kExpectedRandomByteVectors = {
    std::make_pair(kKeccakId, random_bytes_keccak256)};

template <typename HashT>
class HashChainTest : public ::testing::Test {};

using TestedHashTypes = ::testing::Types<Keccak256>;

TYPED_TEST_CASE(HashChainTest, TestedHashTypes);

TYPED_TEST(HashChainTest, HashChGetRandoms) {
  std::array<std::byte, sizeof(uint64_t)> bytes_1{};
  std::array<std::byte, sizeof(uint64_t)> bytes_2{};

  HashChain<TypeParam> hash_ch_1 = HashChain<TypeParam>(bytes_1);
  HashChain<TypeParam> hash_ch_2 = HashChain<TypeParam>(bytes_2);
  TypeParam stat1 = hash_ch_1.GetHashChainState();
  hash_ch_1.GetRandomBytes(bytes_1);
  hash_ch_2.GetRandomBytes(bytes_2);

  for (int i = 0; i < 1000; ++i) {
    hash_ch_1.GetRandomBytes(bytes_1);
    hash_ch_2.GetRandomBytes(bytes_2);
  }

  EXPECT_EQ(stat1, hash_ch_1.GetHashChainState());
  EXPECT_EQ(stat1, hash_ch_2.GetHashChainState());
  EXPECT_EQ(bytes_1, bytes_2);
}

TYPED_TEST(HashChainTest, PyHashChainUpdateParity) {
  const size_t hash_id = typeid(TypeParam).hash_code();

  const std::array<std::byte, 8> dead_beef_bytes =
      MakeByteArray<0x00, 0x00, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF>();
  const std::array<std::byte, 8> daba_daba_da_bytes =
      MakeByteArray<0x00, 0x00, 0x00, 0xDA, 0xBA, 0xDA, 0xBA, 0xDA>();

  std::array<std::byte, sizeof(uint64_t)> bytes_1{};
  auto hash_ch = HashChain<TypeParam>(dead_beef_bytes);

  hash_ch.GetRandomBytes(bytes_1);
  std::vector<std::byte> bytes_1v(bytes_1.begin(), bytes_1.end());
  EXPECT_EQ(kExpectedRandomByteVectors.at(hash_id).at(1), bytes_1v);

  for (int i = 1; i < 1000; ++i) {
    hash_ch.GetRandomBytes(bytes_1);
  }
  bytes_1v = std::vector<std::byte>(bytes_1.begin(), bytes_1.end());
  EXPECT_EQ(kExpectedRandomByteVectors.at(hash_id).at(1000), bytes_1v);

  hash_ch.UpdateHashChain(daba_daba_da_bytes);
  hash_ch.GetRandomBytes(bytes_1);

  bytes_1v = std::vector<std::byte>(bytes_1.begin(), bytes_1.end());
  EXPECT_EQ(kExpectedRandomByteVectors.at(hash_id).at(1001), bytes_1v);
}

// Ensure <Keccak256> HashChain is initialized identically to the
// python counter part.
TEST(HashChain, Keccak256HashChInitUpdate) {
  const std::array<std::byte, 12> k_hello_world =
      MakeByteArray<'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'>();

  HashChain<Keccak256> hash_ch_1(k_hello_world);
  HashChain<Keccak256> hash_ch_2 = HashChain<Keccak256>();
  ASSERT_NE(hash_ch_2.GetHashChainState(), hash_ch_1.GetHashChainState());
  const std::array<std::byte, Keccak256::kDigestNumBytes> exp_hw_hash = MakeByteArray<
      0x3E, 0xA2, 0xF1, 0xD0, 0xAB, 0xF3, 0xFC, 0x66, 0xCF, 0x29, 0xEE, 0xBB, 0x70, 0xCB, 0xD4,
      0xE7, 0xFE, 0x76, 0x2E, 0xF8, 0xA0, 0x9B, 0xCC, 0x06, 0xC8, 0xED, 0xF6, 0x41, 0x23, 0x0A,
      0xFE, 0xC0>();
  ASSERT_EQ(exp_hw_hash, hash_ch_1.GetHashChainState().GetDigest());
}

}  // namespace
}  // namespace starkware
