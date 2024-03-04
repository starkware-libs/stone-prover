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

#include "starkware/channel/noninteractive_verifier_felt_channel.h"

#include <endian.h>
#include <array>
#include <string>
#include <utility>

#include "glog/logging.h"

#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/noninteractive_channel_utils.h"
#include "starkware/channel/proof_of_work.h"
#include "starkware/crypt_tools/invoke.h"
#include "starkware/crypt_tools/template_instantiation.h"

namespace starkware {

std::vector<std::byte> NoninteractiveVerifierFeltChannel::GetStateBytesVector() {
  std::vector<std::byte> raw_bytes(FeltSizeInBytes);
  auto raw_bytes_span = gsl::make_span(raw_bytes);
  state_.ToBytesStandardForm(raw_bytes_span.subspan(0, FeltSizeInBytes));
  return raw_bytes;
}

gsl::span<const FieldElementT> NoninteractiveVerifierFeltChannel::ReceiveFieldElements(
    size_t n_felts) {
  // Hash the entire field elements that were sent, and then its result with the state and
  // update it. As the prover sent data, the counter is then reset.
  if (in_query_phase_) {
    return gsl::make_span(proof_).subspan(proof_read_index_, n_felts);
  }
  std::vector<FieldElementT> felts;
  felts.reserve(n_felts + 1);
  felts.insert(felts.begin(), (state_ + FieldElementT::One()));
  felts.insert(
      felts.end(), proof_.begin() + proof_read_index_,
      proof_.begin() + proof_read_index_ + n_felts);

  // Update the state and reset the counter, as the prover sent through the channel.
  gsl::span<const FieldElementT> felts_span = gsl::make_span(felts);
  state_ = HashT::HashFeltsWithLength(felts_span);
  counter_ = FieldElementT::Zero();
  return gsl::make_span(proof_).subspan(proof_read_index_, n_felts);
}

void NoninteractiveVerifierFeltChannel::SendBytes(const gsl::span<const std::byte> /*raw_bytes*/) {
  // For the non-interactive verifier implementation this function does nothing.
  // Any updates to the hash chain are the responsibility of functions requiring randomness.
  ASSERT_RELEASE(!in_query_phase_, "Verifier can't send randomness after query phase has begun.");
}

void NoninteractiveVerifierFeltChannel::ReceiveFieldElementSpanImpl(
    const Field& field, const FieldElementSpan& span) {
  ASSERT_RELEASE(
      field.IsOfType<FieldElementT>(),
      "This configuration is only supported for PrimeFieldElement<252, 0>");
  const size_t n_felts = span.Size();
  ASSERT_RELEASE((proof_read_index_ + n_felts) <= proof_.size(), "Proof too short.");

  gsl::span<const FieldElementT> felts_received = ReceiveFieldElements(n_felts);

  auto output_span = span.As<FieldElementT>();
  for (size_t i = 0; i < n_felts; i++) {
    output_span.at(i) = felts_received[i];
  }

  proof_read_index_ += n_felts;
  proof_statistics_.byte_count += (n_felts * FeltSizeInBytes);
}

std::vector<std::byte> NoninteractiveVerifierFeltChannel::ReceiveBytes(const size_t num_bytes) {
  // Convert bytes to FieldElementTs, enforcing the rule of handling all of the internal data as
  // FieldElementTs.
  ASSERT_RELEASE(num_bytes % FeltSizeInBytes == 0, "Bad input legnth.");
  const size_t n_felts = num_bytes / FeltSizeInBytes;
  ASSERT_RELEASE(proof_read_index_ + n_felts <= proof_.size(), "Proof too short.");

  gsl::span<const FieldElementT> felts_received = ReceiveFieldElements(n_felts);

  // Write the received felts into the output buffer.
  std::vector<std::byte> raw_bytes(num_bytes);
  auto raw_bytes_span = gsl::make_span(raw_bytes);
  for (size_t i = 0; i < n_felts; i++) {
    felts_received[i].ToBytesStandardForm(
        raw_bytes_span.subspan(i * FeltSizeInBytes, FeltSizeInBytes));
  }

  proof_read_index_ += n_felts;
  proof_statistics_.byte_count += num_bytes;

  return raw_bytes;
}

uint64_t NoninteractiveVerifierFeltChannel::GetRandomNumber(uint64_t upper_bound) {
  ASSERT_RELEASE(IsPowerOfTwo(upper_bound), "Value of upper_bound argument must be a power of 2.");
  FieldElementT random_felt = HashT::Hash(state_, counter_);
  counter_ = counter_ + FieldElementT::One();
  FieldElementT::ValueType raw_random_felt = random_felt.ToStandardForm();
  return raw_random_felt[0] % upper_bound;
}

FieldElement NoninteractiveVerifierFeltChannel::GetRandomFieldElement(const Field& field) {
  ASSERT_RELEASE(
      field.IsOfType<FieldElementT>(),
      "This configuration is only supported for PrimeFieldElement<252, 0>");
  ASSERT_RELEASE(!in_query_phase_, "Verifier can't send randomness after query phase has begun.");
  VLOG(4) << "Prng state: " << BytesToHexString(GetStateBytesVector());
  FieldElement next_felt_to_prover = FieldElement(HashT::Hash(state_, counter_));
  counter_ = counter_ + FieldElementT::One();
  return next_felt_to_prover;
}

void NoninteractiveVerifierFeltChannel::ApplyProofOfWork(size_t security_bits) {
  if (security_bits == 0) {
    return;
  }

  AnnotationScope scope(this, "Proof of Work");
  const auto prev_state = GetStateBytesVector();

  // Read the nonce in chunk equal in size to felt.
  std::vector<std::byte> witness_long = ReceiveData(FeltSizeInBytes, "POW");

  InvokeByHashFunc(pow_hash_name_, [&](auto hash_tag) {
    using POWHashT = typename decltype(hash_tag)::type;
    ProofOfWorkVerifier<POWHashT> pow_verifier;

    // Take only kNonceBytes of the trailing bytes.
    std::vector<std::byte> witness = {
        witness_long.end() - ProofOfWorkVerifier<POWHashT>::kNonceBytes, witness_long.end()};
    ASSERT_RELEASE(pow_verifier.Verify(prev_state, security_bits, witness), "Wrong proof of work");
  });
}

bool NoninteractiveVerifierFeltChannel::IsEndOfProof() {
  return proof_read_index_ >= proof_.size();
}

}  // namespace starkware
