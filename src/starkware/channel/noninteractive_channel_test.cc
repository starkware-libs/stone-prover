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

#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"

#include <cstddef>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polymorphic/field.h"
#include "starkware/crypt_tools/blake2s.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {
namespace {

using testing::HasSubstr;

template <typename T>
class NoninteractiveChannelTest : public ::testing::Test {
 public:
  Prng prng;
  const Prng channel_prng{};
  std::vector<std::byte> RandomByteVector(size_t length) { return prng.RandomByteVector(length); }
};

using TestedChannelTypes = ::testing::Types<Blake2s256>;
TYPED_TEST_CASE(NoninteractiveChannelTest, TestedChannelTypes);

// This test uses constants, to serve as a reference for other implementations.
TYPED_TEST(NoninteractiveChannelTest, ConstantKeccakChannelTest) {
  using FieldElementT = PrimeFieldElement<252, 0>;
  Prng prng(MakeByteArray<0x0, 0x0, 0x0, 0x0>());

  NoninteractiveProverChannel prover_channel(prng.Clone());

  NoninteractiveVerifierChannel verifier_channel(prng.Clone(), prover_channel.GetProof());

  {
    const FieldElementT res =
        verifier_channel.GetRandomFieldElement(Field::Create<FieldElementT>()).As<FieldElementT>();
    FieldElementT expected = FieldElementT::FromBigInt(
        0x7f097aaa40a3109067011986ae40f1ce97a01f4f1a72d80a52821f317504992_Z);
    EXPECT_EQ(res, expected);
  }

  {
    const FieldElementT res =
        verifier_channel.GetRandomFieldElement(Field::Create<FieldElementT>()).As<FieldElementT>();
    FieldElementT expected = FieldElementT::FromBigInt(
        0x18bcafdd60fc70e5e8a9a18687135d0bf1a355d9882969a6b3619e56bf2d49d_Z);
    EXPECT_EQ(res, expected);
  }

  {
    const FieldElementT res =
        verifier_channel.GetRandomFieldElement(Field::Create<FieldElementT>()).As<FieldElementT>();
    FieldElementT expected = FieldElementT::FromBigInt(
        0x2f06b17e08bc409b945b951de8102653dc48a143b87d09b6c95587679816d02_Z);
    EXPECT_EQ(res, expected);
  }
}

TYPED_TEST(NoninteractiveChannelTest, SendingConsistentWithReceivingBytes) {
  NoninteractiveProverChannel prover_channel(this->channel_prng.Clone());

  std::vector<std::byte> pdata1 = this->RandomByteVector(8);
  prover_channel.SendBytes(gsl::make_span(pdata1).as_span<std::byte>());
  std::vector<std::byte> pdata2 = this->RandomByteVector(4);
  prover_channel.SendBytes(gsl::make_span(pdata2).as_span<std::byte>());

  NoninteractiveVerifierChannel verifier_channel(
      this->channel_prng.Clone(), prover_channel.GetProof());

  std::vector<std::byte> vdata1(verifier_channel.ReceiveBytes(8));
  EXPECT_EQ(vdata1, pdata1);
  std::vector<std::byte> vdata2(verifier_channel.ReceiveBytes(4));
  EXPECT_EQ(vdata2, pdata2);
}

TYPED_TEST(NoninteractiveChannelTest, ProofOfWork) {
  NoninteractiveProverChannel prover_channel(this->channel_prng.Clone());

  const size_t work_bits = 15;
  prover_channel.ApplyProofOfWork(work_bits);
  uint64_t pow_value = prover_channel.ReceiveNumber(Pow2(24));

  // Completeness.
  NoninteractiveVerifierChannel verifier_channel(
      this->channel_prng.Clone(), prover_channel.GetProof());

  verifier_channel.ApplyProofOfWork(work_bits);
  EXPECT_EQ(verifier_channel.GetAndSendRandomNumber(Pow2(24)), pow_value);

  // Soundness.
  NoninteractiveVerifierChannel verifier_channel_bad_1(
      this->channel_prng.Clone(), prover_channel.GetProof());
  EXPECT_ASSERT(
      verifier_channel_bad_1.ApplyProofOfWork(work_bits + 1), HasSubstr("Wrong proof of work"));

  NoninteractiveVerifierChannel verifier_channel_bad2(
      this->channel_prng.Clone(), prover_channel.GetProof());

  // Note this fails with probability 2^{-14}.
  EXPECT_ASSERT(
      verifier_channel_bad2.ApplyProofOfWork(work_bits - 1), HasSubstr("Wrong proof of work"));

  // Check value was actually changed.
  NoninteractiveProverChannel nonpow_prover_channel(this->channel_prng.Clone());
  EXPECT_NE(nonpow_prover_channel.ReceiveNumber(Pow2(24)), pow_value);
}

TYPED_TEST(NoninteractiveChannelTest, ProofOfWorkDependsOnState) {
  NoninteractiveProverChannel prover_channel_1(this->channel_prng.Clone());
  std::vector<std::byte> pdata1 = this->RandomByteVector(8);
  prover_channel_1.SendBytes(gsl::make_span(pdata1).as_span<std::byte>());

  const size_t work_bits = 15;
  prover_channel_1.ApplyProofOfWork(work_bits);
  uint64_t pow_value_1 = prover_channel_1.ReceiveNumber(Pow2(24));

  NoninteractiveProverChannel prover_channel_2(this->channel_prng.Clone());
  std::vector<std::byte> pdata2 = this->RandomByteVector(8);
  prover_channel_2.SendBytes(gsl::make_span(pdata2).as_span<std::byte>());

  prover_channel_2.ApplyProofOfWork(work_bits);
  uint64_t pow_value_2 = prover_channel_2.ReceiveNumber(Pow2(24));

  EXPECT_NE(pow_value_1, pow_value_2);
}

TYPED_TEST(NoninteractiveChannelTest, ProofOfWorkZeroBits) {
  NoninteractiveProverChannel prover_channel_1(this->channel_prng.Clone());

  prover_channel_1.ApplyProofOfWork(0);
  uint64_t pow_value_1 = prover_channel_1.ReceiveNumber(Pow2(24));

  NoninteractiveProverChannel prover_channel_2(this->channel_prng.Clone());
  uint64_t pow_value_2 = prover_channel_2.ReceiveNumber(Pow2(24));

  EXPECT_EQ(pow_value_1, pow_value_2);

  // Verify.
  NoninteractiveVerifierChannel verifier_channel(
      this->channel_prng.Clone(), prover_channel_1.GetProof());

  verifier_channel.ApplyProofOfWork(0);
  uint64_t pow_value_3 = verifier_channel.GetAndSendRandomNumber(Pow2(24));
  EXPECT_EQ(pow_value_1, pow_value_3);
}

TYPED_TEST(NoninteractiveChannelTest, SendingConsistentWithReceivingRandomBytes) {
  NoninteractiveProverChannel prover_channel(this->channel_prng.Clone());
  std::vector<std::vector<std::byte>> bytes_sent{};

  for (size_t i = 0; i < 100; ++i) {
    auto random_num = this->prng.template UniformInt<uint64_t>(0, 128);
    std::vector<std::byte> bytes_to_send = this->prng.RandomByteVector(random_num);
    prover_channel.SendBytes(bytes_to_send);
    bytes_sent.push_back(bytes_to_send);
  }

  NoninteractiveVerifierChannel verifier_channel(
      this->channel_prng.Clone(), prover_channel.GetProof());
  for (const std::vector<std::byte>& bytes : bytes_sent) {
    ASSERT_EQ(verifier_channel.ReceiveBytes(bytes.size()), bytes);
  }
}

TYPED_TEST(NoninteractiveChannelTest, RandomDataConsistency) {
  NoninteractiveProverChannel prover_channel(this->channel_prng.Clone());

  Field test_field = Field::Create<TestFieldElement>();
  Field long_field = Field::Create<LongFieldElement>();
  Field prime_field = Field::Create<PrimeFieldElement<252, 0>>();

  uint64_t pnumber = prover_channel.ReceiveNumber(1000);
  FieldElement ptest_field_element = prover_channel.ReceiveFieldElement(test_field);
  FieldElement plong_field_element = prover_channel.ReceiveFieldElement(long_field);
  FieldElement pprime_field_element = prover_channel.ReceiveFieldElement(prime_field);
  std::vector<std::byte> proof(prover_channel.GetProof());

  NoninteractiveVerifierChannel verifier_channel(this->channel_prng.Clone(), proof);
  uint64_t vnumber = verifier_channel.GetAndSendRandomNumber(1000);
  EXPECT_EQ(vnumber, pnumber);
  FieldElement vtest_field_element = verifier_channel.GetAndSendRandomFieldElement(test_field);
  EXPECT_EQ(vtest_field_element, ptest_field_element);
  FieldElement vlong_field_element = verifier_channel.GetAndSendRandomFieldElement(long_field);
  EXPECT_EQ(vlong_field_element, plong_field_element);
  FieldElement vprime_field_element = verifier_channel.GetAndSendRandomFieldElement(prime_field);
  EXPECT_EQ(vprime_field_element, pprime_field_element);
}

TYPED_TEST(NoninteractiveChannelTest, SendReceiveConsistency) {
  NoninteractiveProverChannel prover_channel(this->channel_prng.Clone());
  Field test_field = Field::Create<TestFieldElement>();
  Field long_field = Field::Create<LongFieldElement>();
  Field prime_field = Field::Create<PrimeFieldElement<252, 0>>();
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());

  FieldElement pelem1 = test_field.RandomElement(&prng);
  FieldElement pelem2 = prime_field.RandomElement(&prng);
  FieldElement pelem3 = long_field.RandomElement(&prng);
  Blake2s256 pcommitment1 = prng.RandomHash<Blake2s256>();

  prover_channel.SendFieldElement(pelem1);
  prover_channel.SendFieldElement(pelem2);
  prover_channel.SendFieldElement(pelem3);
  prover_channel.SendCommitmentHash(pcommitment1);

  std::vector<std::byte> proof(prover_channel.GetProof());

  NoninteractiveVerifierChannel verifier_channel(this->channel_prng.Clone(), proof);

  EXPECT_FALSE(verifier_channel.IsEndOfProof());
  FieldElement velem1 = verifier_channel.ReceiveFieldElement(test_field);
  FieldElement velem2 = verifier_channel.ReceiveFieldElement(prime_field);
  FieldElement velem3 = verifier_channel.ReceiveFieldElement(long_field);
  Blake2s256 vcommitment1 = verifier_channel.template ReceiveCommitmentHash<Blake2s256>();
  EXPECT_TRUE(verifier_channel.IsEndOfProof());

  EXPECT_EQ(velem1, pelem1);
  EXPECT_EQ(velem2, pelem2);
  EXPECT_EQ(velem3, pelem3);
  EXPECT_EQ(vcommitment1, pcommitment1);

  // Attempt to read beyond the end of the proof.
  EXPECT_ASSERT(
      verifier_channel.template ReceiveCommitmentHash<Blake2s256>(), HasSubstr("Proof too short."));
}

/*
  This test mimics the expected behavior of a FRI prover while using the channel.
  This is done without integration with the FRI implementation, as a complement to the FRI test
  using a gmock of the channel.
  Semantics of the information sent and received are hence merely a behavioral approximation of
  what will take place in a real scenario. Nevertheless, this test is expected to cover the
  typical usage flow of a STARK/FRI proof protocol.
*/
TYPED_TEST(NoninteractiveChannelTest, FriFlowSimulation) {
  Field test_field = Field::Create<TestFieldElement>();

  NoninteractiveProverChannel prover_channel(this->channel_prng.Clone());

  TypeParam pcommitment1 = this->prng.template RandomHash<TypeParam>();
  prover_channel.template SendCommitmentHash<TypeParam>(pcommitment1, "First FRI layer");

  FieldElement ptest_field_element1 =
      prover_channel.ReceiveFieldElement(test_field, "evaluation point");
  FieldElement ptest_field_element2 =
      prover_channel.ReceiveFieldElement(test_field, "2nd evaluation point");

  FieldElement pexpected_last_layer_const = test_field.RandomElement(&this->prng);

  prover_channel.SendFieldElement(pexpected_last_layer_const, "expected last layer const");

  uint64_t pnumber1 = prover_channel.ReceiveNumber(8, "query index #1 first layer");
  uint64_t pnumber2 = prover_channel.ReceiveNumber(8, "query index #2 first layer");

  std::vector<TypeParam> pdecommitment1;
  for (size_t i = 0; i < 15; ++i) {
    pdecommitment1.push_back(this->prng.template RandomHash<TypeParam>());
    prover_channel.SendDecommitmentNode<TypeParam>(pdecommitment1.back(), "FRI layer");
  }

  std::vector<std::byte> proof(prover_channel.GetProof());

  NoninteractiveVerifierChannel verifier_channel(this->channel_prng.Clone(), proof);

  TypeParam vcommitment1 =
      verifier_channel.template ReceiveCommitmentHash<TypeParam>("First FRI layer");
  EXPECT_EQ(vcommitment1, pcommitment1);
  FieldElement vtest_field_element1 =
      verifier_channel.GetAndSendRandomFieldElement(test_field, "evaluation point");
  EXPECT_EQ(vtest_field_element1, ptest_field_element1);
  FieldElement vtest_field_element2 =
      verifier_channel.GetAndSendRandomFieldElement(test_field, "evaluation point ^ 2");
  EXPECT_EQ(vtest_field_element2, ptest_field_element2);
  FieldElement vexpected_last_layer_const =
      verifier_channel.ReceiveFieldElement(test_field, "expected last layer const");
  EXPECT_EQ(vexpected_last_layer_const, pexpected_last_layer_const);
  uint64_t vnumber1 = verifier_channel.GetAndSendRandomNumber(8, "query index #1 first layer");
  EXPECT_EQ(vnumber1, pnumber1);
  uint64_t vnumber2 = verifier_channel.GetAndSendRandomNumber(8, "query index #2 first layer");
  EXPECT_EQ(vnumber2, pnumber2);
  std::vector<TypeParam> vdecommitment1;
  for (size_t i = 0; i < 15; ++i) {
    vdecommitment1.push_back(verifier_channel.ReceiveDecommitmentNode<TypeParam>("FRI layer"));
  }
  EXPECT_EQ(vdecommitment1, pdecommitment1);

  LOG(INFO) << "\n";
  LOG(INFO) << prover_channel;
  LOG(INFO) << "\n";
  LOG(INFO) << verifier_channel;
}

}  // namespace
}  // namespace starkware
