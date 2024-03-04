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

#include "starkware/channel/noninteractive_prover_felt_channel.h"

#include <endian.h>
#include <array>
#include <cstddef>
#include <string>
#include <utility>

#include "glog/logging.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/noninteractive_channel_utils.h"
#include "starkware/channel/proof_of_work.h"
#include "starkware/crypt_tools/invoke.h"
#include "starkware/crypt_tools/template_instantiation.h"
#include "starkware/randomness/prng.h"

namespace starkware {

std::vector<std::byte> NoninteractiveProverFeltChannel::GetStateBytesVector() {
  std::vector<std::byte> raw_bytes(FeltSizeInBytes);
  auto raw_bytes_span = gsl::make_span(raw_bytes);

  state_.ToBytesStandardForm(raw_bytes_span.subspan(0, FeltSizeInBytes));
  return raw_bytes;
}

void NoninteractiveProverFeltChannel::InnerSendFieldElements(
    gsl::span<const FieldElementT> elements) {
  std::vector<FieldElementT> felts;
  felts.reserve(elements.size() + 1);

  felts.insert(felts.begin(), (state_ + FieldElementT::One()));
  felts.insert(felts.end(), elements.begin(), elements.end());

  // Update the state and reset the counter, as the prover sent through the channel.
  gsl::span<const FieldElementT> felts_span = gsl::make_span(felts);
  state_ = Poseidon3::HashFeltsWithLength(felts_span);
  counter_ = FieldElementT::Zero();
}

void NoninteractiveProverFeltChannel::SendBytes(const gsl::span<const std::byte> raw_bytes) {
  // Convert bytes to FieldElementTs, enforcing the rule of handling all of the internal data as
  // FieldElementTs.
  ASSERT_RELEASE(raw_bytes.size() % FeltSizeInBytes == 0, "Bad input legnth.");
  size_t n_elements = raw_bytes.size() / FeltSizeInBytes;
  std::vector<FieldElementT> felts;

  for (size_t elem_idx = 0; elem_idx < n_elements; elem_idx++) {
    ValueType element =
        ValueType::FromBytes(raw_bytes.subspan(elem_idx * FeltSizeInBytes, FeltSizeInBytes));
    ASSERT_RELEASE(
        element < FieldElementT::GetModulus(), "The input must be smaller than the field prime.");
    FieldElementT fieldElement = FieldElementT::FromBigInt(element);
    proof_.push_back(fieldElement);
  }

  if (!in_query_phase_) {
    // Insert (state_ + 1) to that start, then hash the entire vector and update the state_ to its
    // new value. As the prover sent data, the counter is then reset.
    InnerSendFieldElements(gsl::make_span(proof_).subspan(proof_.size() - n_elements, n_elements));
  }
  proof_statistics_.byte_count += raw_bytes.size();
}

void NoninteractiveProverFeltChannel::SendFieldElementSpanImpl(
    const ConstFieldElementSpan& values) {
  ASSERT_RELEASE(
      (values.GetField().IsOfType<FieldElementT>()),
      "This configuration is only supported for PrimeFieldElement<252, 0>");
  auto values_span = values.As<FieldElementT>();
  for (size_t i = 0; i < values_span.size(); i++) {
    proof_.push_back(values_span[i]);
  }

  if (!in_query_phase_) {
    // Hash the entire field elements that were sent, and then its result with the state and
    // update it. As the prover sent data, the counter is then reset.
    InnerSendFieldElements(values_span);
  }
  proof_statistics_.byte_count += values.Size() * FeltSizeInBytes;
}

FieldElement NoninteractiveProverFeltChannel::ReceiveFieldElementImpl(const Field& field) {
  ASSERT_RELEASE(!in_query_phase_, "Prover can't receive randomness after query phase has begun.");
  ASSERT_RELEASE(
      (field.IsOfType<FieldElementT>()),
      "This configuration is only supported for PrimeFieldElement<252, 0>");
  VLOG(4) << "Prng state: " << BytesToHexString(GetStateBytesVector());
  FieldElement felt_from_verifier = FieldElement(HashT::Hash(state_, counter_));
  counter_ = counter_ + FieldElementT::One();
  return felt_from_verifier;
}

/*
  Receives a random number from the verifier. The number should be chosen uniformly in the range
  [0, upper_bound).
*/
uint64_t NoninteractiveProverFeltChannel::ReceiveNumberImpl(uint64_t upper_bound) {
  ASSERT_RELEASE(IsPowerOfTwo(upper_bound), "Value of upper_bound argument must be a power of 2.");
  FieldElementT random_felt = HashT::Hash(state_, counter_);
  counter_ = counter_ + FieldElementT::One();
  FieldElementT::ValueType raw_random_felt = random_felt.ToStandardForm();
  return raw_random_felt[0] % upper_bound;
}

void NoninteractiveProverFeltChannel::ApplyProofOfWork(size_t security_bits) {
  if (security_bits == 0) {
    return;
  }

  AnnotationScope scope(this, "Proof of Work");
  std::vector<std::byte> proof_of_work = InvokeByHashFunc(pow_hash_name_, [&](auto hash_tag) {
    using POWHashT = typename decltype(hash_tag)::type;
    ASSERT_RELEASE(
        ProofOfWorkVerifier<POWHashT>::kNonceBytes <= FeltSizeInBytes,
        "Nonce size has to be smaller the the number of bytes of the channel's state.");
    ProofOfWorkProver<POWHashT> pow_prover;
    return pow_prover.Prove(GetStateBytesVector(), security_bits);
  });

  // Expand the nonce to be compatible with the size of felt.
  std::vector<std::byte> padded_proof_of_work(FeltSizeInBytes, std::byte(0x00));
  std::copy(
      proof_of_work.begin(), proof_of_work.end(),
      padded_proof_of_work.end() - proof_of_work.size());

  SendData(padded_proof_of_work, "POW");
}

std::vector<std::byte> NoninteractiveProverFeltChannel::GetProof() const {
  std::vector<std::byte> bytes_proof(proof_.size() * FeltSizeInBytes);
  auto bytes_proof_span = gsl::make_span(bytes_proof);
  for (size_t i = 0; i < proof_.size(); i++) {
    // Write to proofs field element bytes in standard form.
    proof_[i].ToBytesStandardForm(bytes_proof_span.subspan(i * FeltSizeInBytes, FeltSizeInBytes));
  }
  return bytes_proof;
}

}  // namespace starkware
