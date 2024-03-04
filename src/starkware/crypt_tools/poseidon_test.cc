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

#include "starkware/crypt_tools/poseidon.h"

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/crypt_tools/test_utils.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::HasSubstr;

Poseidon3 AsDigest(const BigInt<4> val) {
  std::array<std::byte, BigInt<4>::SizeInBytes()> bytes{};
  Serialize(val, bytes, /*use_big_endian*/ true);
  return Poseidon3::InitDigestTo(bytes);
}

TEST(Poseidon3, CheckInitDigestTo) {
  Prng prng;
  FieldElementT random_field_element = FieldElementT::RandomElement(&prng);
  FieldElementT another_random_field_element = FieldElementT::RandomElement(&prng);

  std::array<std::byte, Poseidon3::kDigestNumBytes> random_field_element_as_bytes;
  random_field_element.ToBytesStandardForm(random_field_element_as_bytes);

  Poseidon3 poseidon3_random_digest = Poseidon3::InitDigestTo(random_field_element);
  Poseidon3 poseidon3_random_array_digest = Poseidon3::InitDigestTo(random_field_element_as_bytes);
  Poseidon3 poseidon3_another_random_digest = Poseidon3::InitDigestTo(another_random_field_element);

  EXPECT_EQ(poseidon3_random_digest, poseidon3_random_array_digest);
  EXPECT_NE(poseidon3_random_digest, poseidon3_another_random_digest);

  const BigInt<4> numberBiggerThanPrime =
      0x93A77118133287637EBDCD9E87A1613E443DF789558867F5BA91FAF7A024204_Z;
  EXPECT_ASSERT(
      AsDigest(numberBiggerThanPrime),
      HasSubstr("The input must be smaller than the field prime."));
}

TEST(Poseidon3, Hash) {
  const BigInt<4> val0 = 0x23A77118133287637EBDCD9E87A1613E443DF789558867F5BA91FAF7A024204_Z;
  const BigInt<4> val1 = 0x259F432E6F4590B9A164106CF6A659EB4862B21FB97D43588561712E8E5216A_Z;
  const BigInt<4> changed_val1 =
      0x259F432E6F4590B9A164106CF6A659EB4862B21FB97D43588561712E8E5216B_Z;
  const BigInt<4> res = 0x4BE9AF45B942B4B0C9F04A15E37B7F34F8109873EF7EF20E9EEF8A38A3011E1_Z;

  // Check hash.
  EXPECT_EQ(Poseidon3::Hash(AsDigest(val0), AsDigest(val1)), AsDigest(res));

  // Check that the hash output changes even with a slight change to the input.
  EXPECT_NE(Poseidon3::Hash(AsDigest(val0), AsDigest(changed_val1)), AsDigest(res));
}

TEST(Poseidon3, HashFieldElements) {
  const BigInt<4> val0 = 0x23A77118133287637EBDCD9E87A1613E443DF789558867F5BA91FAF7A024204_Z;
  const BigInt<4> val1 = 0x259F432E6F4590B9A164106CF6A659EB4862B21FB97D43588561712E8E5216A_Z;
  const BigInt<4> changed_val1 =
      0x259F432E6F4590B9A164106CF6A659EB4862B21FB97D43588561712E8E5216B_Z;
  const BigInt<4> res = 0x4BE9AF45B942B4B0C9F04A15E37B7F34F8109873EF7EF20E9EEF8A38A3011E1_Z;

  // Check hash.
  EXPECT_EQ(
      Poseidon3::Hash(FieldElementT::FromBigInt(val0), FieldElementT::FromBigInt(val1)),
      FieldElementT::FromBigInt(res));

  // Check that the hash output changes even with a slight change to the input.
  EXPECT_NE(
      Poseidon3::Hash(FieldElementT::FromBigInt(val0), FieldElementT::FromBigInt(changed_val1)),
      FieldElementT::FromBigInt(res));
}

TEST(Poseidon3, PoseidonHashBytesWithLength) {
  std::array<std::byte, 5 * BigInt<4>::SizeInBytes()> bytes{};
  gsl::span<std::byte> spn = gsl::make_span(bytes);

  // Put the field elements together in a string.
  // Use values taken from test_poseidon_hash_many_random() in
  // src/starkware/cairo/common/builtin_poseidon/poseidon_test.py.
  Serialize(
      0x6d841133e95d568a98362ae077545482f1b508cb48645f296de72f05f1e6afe_Z,
      spn.subspan(0, BigInt<4>::SizeInBytes()), /*use_big_endian*/ true);
  Serialize(
      0x23a4459537f5b7fa228d32336a00f9b241289e3c69fb3527e0614c4e77bb376_Z,
      spn.subspan(BigInt<4>::SizeInBytes(), BigInt<4>::SizeInBytes()), /*use_big_endian*/ true);
  Serialize(
      0x7c187057c013dd3a6d8ce76683cd7875d236ca396204922122a10a77cef6f2a_Z,
      spn.subspan(2 * BigInt<4>::SizeInBytes(), BigInt<4>::SizeInBytes()), /*use_big_endian*/
      true);
  Serialize(
      0x396f0205dbcda22900af19938d0607f8f9acc9c7a4c8d75fc19b5a1ede874b0_Z,
      spn.subspan(3 * BigInt<4>::SizeInBytes(), BigInt<4>::SizeInBytes()), /*use_big_endian*/
      true);
  Serialize(
      0x48499957377c0df2c671fd1fcb6501b42fb63045ed8d16df08d55ab144e7ab8_Z,
      spn.subspan(4 * BigInt<4>::SizeInBytes(), BigInt<4>::SizeInBytes()), /*use_big_endian*/
      true);

  const BigInt<4> hash_with_bytes_expected =
      0x6c8e43870377cbf91153caed2091bb49ea60401045eccc2e4cc9bbf35218891_Z;
  const BigInt<4> hash_with_bytes_not_expected =
      0x6c8e43870377cbf91153caed2091bb49ea60401045eccc2e4cc9bbf35218892_Z;

  // Check the hash results is equal to expected, and not equal to another unrelated value.
  EXPECT_EQ(Poseidon3::HashBytesWithLength(bytes), AsDigest(hash_with_bytes_expected));
  EXPECT_NE(Poseidon3::HashBytesWithLength(bytes), AsDigest(hash_with_bytes_not_expected));
}

TEST(Poseidon3, PoseidonHashFeltsWithLength) {
  std::vector<FieldElementT> field_elements = {
      FieldElementT::FromBigInt(
          BigInt<4>(0x6d841133e95d568a98362ae077545482f1b508cb48645f296de72f05f1e6afe_Z)),
      FieldElementT::FromBigInt(
          BigInt<4>(0x23a4459537f5b7fa228d32336a00f9b241289e3c69fb3527e0614c4e77bb376_Z)),
      FieldElementT::FromBigInt(
          BigInt<4>(0x7c187057c013dd3a6d8ce76683cd7875d236ca396204922122a10a77cef6f2a_Z)),
      FieldElementT::FromBigInt(
          BigInt<4>(0x396f0205dbcda22900af19938d0607f8f9acc9c7a4c8d75fc19b5a1ede874b0_Z)),
      FieldElementT::FromBigInt(
          BigInt<4>(0x48499957377c0df2c671fd1fcb6501b42fb63045ed8d16df08d55ab144e7ab8_Z)),
  };

  gsl::span<const FieldElementT> field_elements_span = gsl::make_span(field_elements);

  const BigInt<4> hash_with_felts_expected =
      0x6c8e43870377cbf91153caed2091bb49ea60401045eccc2e4cc9bbf35218891_Z;
  const BigInt<4> hash_with_felts_not_expected =
      0x6c8e43870377cbf91153caed2091bb49ea60401045eccc2e4cc9bbf35218892_Z;

  // Check the hash results is equal to expected, and not equal to another unrelated value.
  EXPECT_EQ(
      Poseidon3::HashFeltsWithLength(field_elements_span),
      FieldElementT::FromBigInt(hash_with_felts_expected));
  EXPECT_NE(
      Poseidon3::HashFeltsWithLength(field_elements_span),
      FieldElementT::FromBigInt(hash_with_felts_not_expected));
}

}  // namespace
}  // namespace starkware
