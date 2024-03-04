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
#include "starkware/channel/noninteractive_prover_felt_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/channel/noninteractive_verifier_felt_channel.h"

#include <cstddef>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polymorphic/field.h"
#include "starkware/crypt_tools/blake2s.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/stl_utils/containers.h"

#include "starkware/crypt_tools/poseidon.h"

namespace starkware {
namespace {

using testing::HasSubstr;

using FieldElementT = PrimeFieldElement<252, 0>;

/*
  Uses two hashes, top and bottom, in the commitment.
*/
template <typename ChannelHash, typename PowHash>
struct NoninteractiveVerifierFriendlyChannelT {
  using ChannelHashT = ChannelHash;
  using PowHashT = PowHash;
};

template <typename T>
class NoninteractiveFeltChannelTest : public ::testing::Test {
 public:
  using ChannelHashT = typename T::ChannelHashT;
  using PowHashT = typename T::PowHashT;

  const std::string pow_hash_name = PowHashT::HashName();
  const Field field = Field::Create<FieldElementT>();
  static constexpr size_t kDigestNumBytes = FieldElementT::SizeInBytes();

  Prng prng;
  const Prng channel_prng{};

  std::vector<std::byte> RandomFieldElementVectorAsBytes(size_t length) {
    std::vector<FieldElementT> randon_felts = prng.RandomFieldElementVector<FieldElementT>(length);
    std::vector<std::byte> random_felts_bytes(length * kDigestNumBytes);
    auto random_felts_bytes_span = gsl::make_span(random_felts_bytes);
    for (size_t i = 0; i < length; i++) {
      randon_felts[i].ToBytesStandardForm(
          random_felts_bytes_span.subspan(i * kDigestNumBytes, kDigestNumBytes));
    }
    return random_felts_bytes;
  }

  FieldElementT RandomFieldElement() { return prng.RandomFieldElementVector<FieldElementT>(1)[0]; }

  ChannelHashT RandomHash() {
    return ChannelHashT::InitDigestTo(RandomFieldElementVectorAsBytes(1));
  }
};

using TestedChannelTypes = ::testing::Types<
    NoninteractiveVerifierFriendlyChannelT<Poseidon3, Blake2s256>,
    NoninteractiveVerifierFriendlyChannelT<Poseidon3, Keccak256>>;
TYPED_TEST_CASE(NoninteractiveFeltChannelTest, TestedChannelTypes);

TYPED_TEST(NoninteractiveFeltChannelTest, SendingConsistentWithReceivingBytes) {
  FieldElementT initial_state = this->RandomFieldElement();

  NoninteractiveProverFeltChannel prover_channel(initial_state, this->pow_hash_name);

  // Send one field element as an array of bytes.
  std::vector<std::byte> pdata1 = this->RandomFieldElementVectorAsBytes(1);
  prover_channel.SendBytes(pdata1);

  // Send another two field elements as an array of bytes.
  std::vector<std::byte> pdata2 = this->RandomFieldElementVectorAsBytes(2);
  prover_channel.SendBytes(pdata2);

  // Test fiat-shamir. Receive a field element from the verifier (Note that at this point the
  // verifier does not exist).
  FieldElement pdata3 =
      prover_channel.ReceiveFieldElement(this->field, "Get random field element from verifier.");

  NoninteractiveVerifierFeltChannel verifier_channel(
      initial_state, prover_channel.GetProof(), this->pow_hash_name);

  // Assert the verifier read the first field element properly.
  std::vector<std::byte> vdata1(verifier_channel.ReceiveBytes(this->kDigestNumBytes));
  EXPECT_EQ(vdata1, pdata1);
  // Assert the verifier read the other two field elements properly.
  std::vector<std::byte> vdata2(verifier_channel.ReceiveBytes(2 * this->kDigestNumBytes));
  EXPECT_EQ(vdata2, pdata2);
  // Send a field element to the prover and assert it read it properly.
  FieldElement vdata3 = verifier_channel.GetAndSendRandomFieldElement(
      this->field, "Send random field element to prover.");
  EXPECT_EQ(pdata3, vdata3);
}

TYPED_TEST(NoninteractiveFeltChannelTest, SendingElementsSpanConsistentWithReceiving) {
  // Sends a random n_elements vector using SendFieldElementSpan and make sure the verifier
  // gets the expected vector and that the seed was updated correctly.
  FieldElementT initial_state = this->RandomFieldElement();
  const size_t n_elements = 20;

  NoninteractiveProverFeltChannel prover_channel(initial_state, this->pow_hash_name);

  // Send #n_elements field elements to the verifier, with a single channel update as the operation
  // ends.
  const auto random_vec = FieldElementVector::Make(
      this->prng.template RandomFieldElementVector<FieldElementT>(n_elements));
  auto span = ConstFieldElementSpan(random_vec);
  prover_channel.SendFieldElementSpan(span);
  // Prover reads a random number the verifier sent it, based on the state of the previous send
  // operation.
  uint64_t random_num_p = prover_channel.ReceiveNumber(Pow2(10));

  NoninteractiveVerifierFeltChannel verifier_channel(
      initial_state, prover_channel.GetProof(), this->pow_hash_name);
  // Receive #n_elements field elements from the prover, with a single channel update as the
  // operation ends.
  FieldElementVector verifier_output = FieldElementVector::Make(n_elements, this->field.Zero());
  verifier_channel.ReceiveFieldElementSpan(this->field, verifier_output.AsSpan());
  // Verifier sends a random number to the prover, based on the state of the previous receive
  // operation.
  uint64_t random_num_v = verifier_channel.GetAndSendRandomNumber(Pow2(10));

  // Assert the prover and verifier both had the same interaction.
  EXPECT_EQ(random_vec, verifier_output);
  EXPECT_EQ(random_num_p, random_num_v);
}

TYPED_TEST(NoninteractiveFeltChannelTest, FieldElementSupportSendAndReceiveFieldElementSpan) {
  // Asserts that SendFieldElementSpan and ReceiveFieldElementSpan are supported only
  // for PrimeFieldElement<252, 0>, which is different from TestFieldElement.
  Field field = Field::Create<TestFieldElement>();
  FieldElementT initial_state = this->RandomFieldElement();

  NoninteractiveProverFeltChannel prover_channel(initial_state, this->pow_hash_name);
  const auto random_vec =
      FieldElementVector::Make(this->prng.template RandomFieldElementVector<TestFieldElement>(1));
  EXPECT_ASSERT(
      prover_channel.SendFieldElementSpan(ConstFieldElementSpan(random_vec)),
      HasSubstr("only supported for PrimeFieldElement<252, 0>"));
  NoninteractiveVerifierFeltChannel verifier_channel(
      initial_state, prover_channel.GetProof(), this->pow_hash_name);
  FieldElementVector verifier_output = FieldElementVector::Make(1, field.Zero());
  EXPECT_ASSERT(
      verifier_channel.ReceiveFieldElementSpan(field, verifier_output.AsSpan()),
      HasSubstr("only supported for PrimeFieldElement<252, 0>"));
}

TYPED_TEST(NoninteractiveFeltChannelTest, ProofOfWork) {
  FieldElementT initial_state = this->RandomFieldElement();

  NoninteractiveProverFeltChannel prover_channel(initial_state, this->pow_hash_name);

  const size_t work_bits = 15;
  prover_channel.ApplyProofOfWork(work_bits);
  uint64_t pow_value = prover_channel.ReceiveNumber(Pow2(24));

  // Completeness.
  NoninteractiveVerifierFeltChannel verifier_channel(
      initial_state, prover_channel.GetProof(), this->pow_hash_name);

  verifier_channel.ApplyProofOfWork(work_bits);
  EXPECT_EQ(verifier_channel.GetAndSendRandomNumber(Pow2(24)), pow_value);

  // Soundness.
  NoninteractiveVerifierFeltChannel verifier_channel_bad_1(
      initial_state, prover_channel.GetProof(), this->pow_hash_name);
  EXPECT_ASSERT(
      verifier_channel_bad_1.ApplyProofOfWork(work_bits + 1), HasSubstr("Wrong proof of work"));

  NoninteractiveVerifierFeltChannel verifier_channel_bad_2(
      initial_state, prover_channel.GetProof(), this->pow_hash_name);

  // Note this fails with probability 2^{-14}.
  EXPECT_ASSERT(
      verifier_channel_bad_2.ApplyProofOfWork(work_bits - 1), HasSubstr("Wrong proof of work"));

  // Check value was actually changed.
  NoninteractiveProverFeltChannel nonpow_prover_channel(initial_state, this->pow_hash_name);
  EXPECT_NE(nonpow_prover_channel.ReceiveNumber(Pow2(24)), pow_value);

  // Check ReceiveNumber only accepts 2^n as upper_bound.
  EXPECT_ASSERT(
      prover_channel.ReceiveNumber(Pow2(24) - 1),
      HasSubstr("Value of upper_bound argument must be a power of 2."));
}

/*
  This test mimics the expected behavior of a FRI prover while using the channel.
  This is done without integration with the FRI implementation, as a complement to the FRI test
  using a gmock of the channel.
  Semantics of the information sent and received are hence merely a behavioral approximation of
  what will take place in a real scenario. Nevertheless, this test is expected to cover the
  typical usage flow of a STARK/FRI proof protocol.
*/
TYPED_TEST(NoninteractiveFeltChannelTest, FriFlowSimulation) {
  FieldElementT initial_state = this->RandomFieldElement();

  NoninteractiveProverFeltChannel prover_channel(initial_state, this->pow_hash_name);

  typename TypeParam::ChannelHashT pcommitment1 = this->RandomHash();
  prover_channel.template SendCommitmentHash<typename TypeParam::ChannelHashT>(
      pcommitment1, "First FRI layer");

  FieldElement ptest_field_element1 =
      prover_channel.ReceiveFieldElement(this->field, "evaluation point");
  FieldElement ptest_field_element2 =
      prover_channel.ReceiveFieldElement(this->field, "2nd evaluation point");

  FieldElement pexpected_last_layer_const = this->field.RandomElement(&this->prng);

  prover_channel.SendFieldElement(pexpected_last_layer_const, "expected last layer const");

  uint64_t pnumber1 = prover_channel.ReceiveNumber(8, "query index #1 first layer");
  uint64_t pnumber2 = prover_channel.ReceiveNumber(8, "query index #2 first layer");

  std::vector<typename TypeParam::ChannelHashT> pdecommitment1;
  for (size_t i = 0; i < 15; ++i) {
    pdecommitment1.push_back(this->RandomHash());
    prover_channel.SendDecommitmentNode<typename TypeParam::ChannelHashT>(
        pdecommitment1.back(), "FRI layer");
  }

  std::vector<std::byte> proof(prover_channel.GetProof());

  NoninteractiveVerifierFeltChannel verifier_channel(
      initial_state, prover_channel.GetProof(), this->pow_hash_name);

  typename TypeParam::ChannelHashT vcommitment1 =
      verifier_channel.template ReceiveCommitmentHash<typename TypeParam::ChannelHashT>(
          "First FRI layer");
  EXPECT_EQ(vcommitment1, pcommitment1);
  FieldElement vtest_field_element1 =
      verifier_channel.GetAndSendRandomFieldElement(this->field, "#1 evaluation point");
  EXPECT_EQ(vtest_field_element1, ptest_field_element1);
  FieldElement vtest_field_element2 =
      verifier_channel.GetAndSendRandomFieldElement(this->field, "#2 evaluation point");
  EXPECT_EQ(vtest_field_element2, ptest_field_element2);
  FieldElement vexpected_last_layer_const =
      verifier_channel.ReceiveFieldElement(this->field, "expected last layer const");
  EXPECT_EQ(vexpected_last_layer_const, pexpected_last_layer_const);
  uint64_t vnumber1 = verifier_channel.GetAndSendRandomNumber(8, "query index #1 first layer");
  EXPECT_EQ(vnumber1, pnumber1);
  uint64_t vnumber2 = verifier_channel.GetAndSendRandomNumber(8, "query index #2 first layer");
  EXPECT_EQ(vnumber2, pnumber2);
  std::vector<typename TypeParam::ChannelHashT> vdecommitment1;
  for (size_t i = 0; i < 15; ++i) {
    vdecommitment1.push_back(
        verifier_channel.ReceiveDecommitmentNode<typename TypeParam::ChannelHashT>("FRI layer"));
  }
  EXPECT_EQ(vdecommitment1, pdecommitment1);

  LOG(INFO) << "\n";
  LOG(INFO) << prover_channel;
  LOG(INFO) << "\n";
  LOG(INFO) << verifier_channel;
}

TYPED_TEST(NoninteractiveFeltChannelTest, ProofOfWorkDependsOnState) {
  FieldElementT initial_state = this->RandomFieldElement();

  NoninteractiveProverFeltChannel prover_channel_1(initial_state, this->pow_hash_name);

  // Send a random field element via channel, and apply proof of work.
  std::vector<std::byte> pdata1 = this->RandomFieldElementVectorAsBytes(1);
  prover_channel_1.SendBytes(gsl::make_span(pdata1).as_span<std::byte>());

  const size_t work_bits = 15;
  prover_channel_1.ApplyProofOfWork(work_bits);
  uint64_t pow_value_1 = prover_channel_1.ReceiveNumber(Pow2(24));

  // Send a different random field element via channel, which should affect the channels internal
  // state, and apply proof of work.
  NoninteractiveProverFeltChannel prover_channel_2(initial_state, this->pow_hash_name);
  std::vector<std::byte> pdata2 = this->RandomFieldElementVectorAsBytes(1);
  prover_channel_2.SendBytes(gsl::make_span(pdata2).as_span<std::byte>());

  prover_channel_2.ApplyProofOfWork(work_bits);
  uint64_t pow_value_2 = prover_channel_2.ReceiveNumber(Pow2(24));

  // Assert the two channels returned a different pow value, due to their divergence in previous
  // interaction with the verifier.
  EXPECT_NE(pow_value_1, pow_value_2);
}

TYPED_TEST(NoninteractiveFeltChannelTest, ProofOfWorkZeroBits) {
  FieldElementT initial_state = this->RandomFieldElement();

  NoninteractiveProverFeltChannel prover_channel_1(initial_state, this->pow_hash_name);

  prover_channel_1.ApplyProofOfWork(0);
  uint64_t pow_value_1 = prover_channel_1.ReceiveNumber(Pow2(24));

  NoninteractiveProverFeltChannel prover_channel_2(initial_state, this->pow_hash_name);
  uint64_t pow_value_2 = prover_channel_2.ReceiveNumber(Pow2(24));

  EXPECT_EQ(pow_value_1, pow_value_2);

  // Verify.
  NoninteractiveVerifierFeltChannel verifier_channel(
      initial_state, prover_channel_1.GetProof(), this->pow_hash_name);

  verifier_channel.ApplyProofOfWork(0);
  uint64_t pow_value_3 = verifier_channel.GetAndSendRandomNumber(Pow2(24));
  EXPECT_EQ(pow_value_1, pow_value_3);
}

TYPED_TEST(NoninteractiveFeltChannelTest, SendingConsistentWithReceivingRandomBytes) {
  FieldElementT initial_state = this->RandomFieldElement();

  NoninteractiveProverFeltChannel prover_channel(initial_state, this->pow_hash_name);
  std::vector<std::vector<std::byte>> bytes_sent{};

  for (size_t i = 0; i < 100; ++i) {
    auto random_num = this->prng.template UniformInt<uint64_t>(0, 4);
    std::vector<std::byte> felts_bytes_to_send = this->RandomFieldElementVectorAsBytes(random_num);
    prover_channel.SendBytes(felts_bytes_to_send);
    bytes_sent.push_back(felts_bytes_to_send);
  }

  NoninteractiveVerifierFeltChannel verifier_channel(
      initial_state, prover_channel.GetProof(), this->pow_hash_name);
  for (const std::vector<std::byte>& bytes : bytes_sent) {
    ASSERT_EQ(verifier_channel.ReceiveBytes(bytes.size()), bytes);
  }
}

}  // namespace
}  // namespace starkware
