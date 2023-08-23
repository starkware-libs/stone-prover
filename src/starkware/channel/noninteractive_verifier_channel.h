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

#ifndef STARKWARE_CHANNEL_NONINTERACTIVE_VERIFIER_CHANNEL_H_
#define STARKWARE_CHANNEL_NONINTERACTIVE_VERIFIER_CHANNEL_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/verifier_channel.h"
#include "starkware/randomness/hash_chain.h"

namespace starkware {

class NoninteractiveVerifierChannel : public VerifierChannel {
 public:
  /*
    Initialize the hash chain to a value based on the public input and the constraints system. This
    ensures that the prover doesn't modify the public input after generating the proof.
  */
  explicit NoninteractiveVerifierChannel(
      std::unique_ptr<PrngBase> prng, gsl::span<const std::byte> proof);

  /*
    SendBytes just updates the hash chain but has no other effect.
  */
  void SendBytes(gsl::span<const std::byte> raw_bytes) override;

  /*
    ReceiveBytes reads bytes from the proof and updates the hash chain accordingly.
  */
  std::vector<std::byte> ReceiveBytes(size_t num_bytes) override;

  uint64_t GetRandomNumber(uint64_t upper_bound) override;

  FieldElement GetRandomFieldElement(const Field& field) override;

  /*
    See documentation in noninteractive_prover_channel.h.
  */
  void ApplyProofOfWork(size_t security_bits) override;

  /*
    Returns true if the proof was fully read.
  */
  bool IsEndOfProof();

 private:
  std::unique_ptr<PrngBase> prng_;
  const std::vector<std::byte> proof_;
  size_t proof_read_index_ = 0;
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_NONINTERACTIVE_VERIFIER_CHANNEL_H_
