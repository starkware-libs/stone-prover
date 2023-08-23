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

#ifndef STARKWARE_CHANNEL_NONINTERACTIVE_PROVER_CHANNEL_H_
#define STARKWARE_CHANNEL_NONINTERACTIVE_PROVER_CHANNEL_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/prover_channel.h"
#include "starkware/randomness/hash_chain.h"

namespace starkware {

class NoninteractiveProverChannel : public ProverChannel {
 public:
  /*
    Initialize the hash chain to a value based on the public input and the constraints system. This
    ensures that the prover doesn't modify the public input after generating the proof.
  */
  explicit NoninteractiveProverChannel(std::unique_ptr<PrngBase> prng);

  /*
    SendBytes writes raw bytes to the proof and updates the hash chain.
  */
  void SendBytes(gsl::span<const std::byte> raw_bytes) override;

  /*
    ReceiveBytes is a method that uses a hash chain and every time it is called the requested number
    of random bytes are returned and the hash chain is updated.
  */
  std::vector<std::byte> ReceiveBytes(size_t num_bytes) override;

  FieldElement ReceiveFieldElementImpl(const Field& field) override;

  /*
    Receives a random number from the verifier. The number should be chosen uniformly in the
    range [0, upper_bound).
  */
  uint64_t ReceiveNumberImpl(uint64_t upper_bound) override;

  /*
    This is done using ProofOfWork<Keccak256>. Finds a nonce (8 bytes) for which
      keccak(keccak(magic || prng_seed || work_bits) || nonce)
    has security_bits leading zero bits. Then, appends the nonce to the proof.
  */
  void ApplyProofOfWork(size_t security_bits) override;

  std::vector<std::byte> GetProof() const;

 private:
  std::unique_ptr<PrngBase> prng_;
  std::vector<std::byte> proof_{};
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_NONINTERACTIVE_PROVER_CHANNEL_H_
