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

/*
  Proof of work prover and verifier. The algorithm:
    Find a nonce of size 8 bytes for which hash(hash(magic || seed) || nonce) has work_bits leading
    zero bits (most significant bits).
*/

#ifndef STARKWARE_CHANNEL_PROOF_OF_WORK_H_
#define STARKWARE_CHANNEL_PROOF_OF_WORK_H_

#include <vector>

#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

template <typename HashT>
class ProofOfWorkProver {
  static_assert(HashT::kDigestNumBytes >= 8, "Digest size must be at least 64 bits");

 public:
  /*
    Returns nonce for which hash(hash(magic || seed || work_bits) || nonce) has work_bits leading
    zeros.
  */
  std::vector<std::byte> Prove(
      gsl::span<const std::byte> seed, size_t work_bits, uint64_t log_chunk_size = 20);
  std::vector<std::byte> Prove(
      gsl::span<const std::byte> seed, size_t work_bits, TaskManager* task_manager,
      uint64_t log_chunk_size = 20);
};

template <typename HashT>
class ProofOfWorkVerifier {
  static_assert(HashT::kDigestNumBytes >= 8, "Digest size must be at least 64 bits");

 public:
  static const size_t kNonceBytes = sizeof(uint64_t);

  /*
    Returns true iff hash(hash(magic || seed || work_bits) || nonce) has work_bits leading zeros.
  */
  bool Verify(
      gsl::span<const std::byte> seed, size_t work_bits, gsl::span<const std::byte> nonce_bytes);
};

}  // namespace starkware

#include "starkware/channel/proof_of_work.inl"

#endif  // STARKWARE_CHANNEL_PROOF_OF_WORK_H_
