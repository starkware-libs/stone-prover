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

NoninteractiveProverChannel::NoninteractiveProverChannel(std::unique_ptr<PrngBase> prng)
    : prng_(std::move(prng)) {}

void NoninteractiveProverChannel::SendBytes(const gsl::span<const std::byte> raw_bytes) {
  for (std::byte raw_byte : raw_bytes) {
    proof_.push_back(raw_byte);
  }
  if (!in_query_phase_) {
    prng_->MixSeedWithBytes(raw_bytes);
  }
  proof_statistics_.byte_count += raw_bytes.size();
}

std::vector<std::byte> NoninteractiveProverChannel::ReceiveBytes(const size_t num_bytes) {
  ASSERT_RELEASE(!in_query_phase_, "Prover can't receive randomness after query phase has begun.");
  std::vector<std::byte> bytes(num_bytes);
  prng_->GetRandomBytes(bytes);
  return bytes;
}

FieldElement NoninteractiveProverChannel::ReceiveFieldElementImpl(const Field& field) {
  ASSERT_RELEASE(!in_query_phase_, "Prover can't receive randomness after query phase has begun.");
  VLOG(4) << "Prng state: " << BytesToHexString(prng_->GetPrngState());
  return field.RandomElement(prng_.get());
}

/*
  Receives a random number from the verifier. The number should be chosen uniformly in the range
  [0, upper_bound).
*/
uint64_t NoninteractiveProverChannel::ReceiveNumberImpl(uint64_t upper_bound) {
  ASSERT_RELEASE(!in_query_phase_, "Prover can't receive randomness after query phase has begun.");
  return NoninteractiveChannelUtils::GetRandomNumber(upper_bound, prng_.get());
}

void NoninteractiveProverChannel::ApplyProofOfWork(size_t security_bits) {
  if (security_bits == 0) {
    return;
  }

  AnnotationScope scope(this, "Proof of Work");

  std::vector<std::byte> proof_of_work = InvokeByHashFunc(prng_->GetHashName(), [&](auto hash_tag) {
    using HashT = typename decltype(hash_tag)::type;
    ProofOfWorkProver<HashT> pow_prover;
    return pow_prover.Prove(prng_->GetPrngState(), security_bits);
  });
  SendData(proof_of_work, "POW");
}

std::vector<std::byte> NoninteractiveProverChannel::GetProof() const { return proof_; }

}  // namespace starkware
