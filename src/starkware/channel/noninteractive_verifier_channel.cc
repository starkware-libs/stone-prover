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

#include "starkware/channel/noninteractive_verifier_channel.h"

#include <array>
#include <utility>

#include "glog/logging.h"

#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/noninteractive_channel_utils.h"
#include "starkware/channel/proof_of_work.h"
#include "starkware/crypt_tools/invoke.h"
#include "starkware/crypt_tools/template_instantiation.h"

namespace starkware {

NoninteractiveVerifierChannel::NoninteractiveVerifierChannel(
    std::unique_ptr<PrngBase> prng, const gsl::span<const std::byte> proof)
    : prng_(std::move(prng)), proof_(proof.begin(), proof.end()) {}

void NoninteractiveVerifierChannel::SendBytes(const gsl::span<const std::byte> /*raw_bytes*/) {
  // For the non-interactive verifier implementation this function does nothing.
  // Any updates to the hash chain are the responsibility of functions requiring randomness.
  ASSERT_RELEASE(!in_query_phase_, "Verifier can't send randomness after query phase has begun.");
}

std::vector<std::byte> NoninteractiveVerifierChannel::ReceiveBytes(const size_t num_bytes) {
  ASSERT_RELEASE(proof_read_index_ + num_bytes <= proof_.size(), "Proof too short.");
  std::vector<std::byte> raw_bytes(
      proof_.begin() + proof_read_index_, proof_.begin() + proof_read_index_ + num_bytes);
  proof_read_index_ += num_bytes;
  if (!in_query_phase_) {
    prng_->MixSeedWithBytes(raw_bytes);
  }
  proof_statistics_.byte_count += raw_bytes.size();
  return raw_bytes;
}

uint64_t NoninteractiveVerifierChannel::GetRandomNumber(uint64_t upper_bound) {
  ASSERT_RELEASE(!in_query_phase_, "Verifier can't send randomness after query phase has begun.");
  return NoninteractiveChannelUtils::GetRandomNumber(upper_bound, prng_.get());
}

FieldElement NoninteractiveVerifierChannel::GetRandomFieldElement(const Field& field) {
  ASSERT_RELEASE(!in_query_phase_, "Verifier can't send randomness after query phase has begun.");
  VLOG(4) << "Prng state: " << BytesToHexString(prng_->GetPrngState());
  return field.RandomElement(prng_.get());
}

void NoninteractiveVerifierChannel::ApplyProofOfWork(size_t security_bits) {
  if (security_bits == 0) {
    return;
  }

  AnnotationScope scope(this, "Proof of Work");
  const auto prev_state = prng_->GetPrngState();

  InvokeByHashFunc(prng_->GetHashName(), [&](auto hash_tag) {
    using HashT = typename decltype(hash_tag)::type;
    ProofOfWorkVerifier<HashT> pow_verifier;
    auto witness = ReceiveData(ProofOfWorkVerifier<HashT>::kNonceBytes, "POW");
    ASSERT_RELEASE(pow_verifier.Verify(prev_state, security_bits, witness), "Wrong proof of work");
  });
}

bool NoninteractiveVerifierChannel::IsEndOfProof() { return proof_read_index_ >= proof_.size(); }

}  // namespace starkware
