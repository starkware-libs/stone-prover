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

#ifndef STARKWARE_CHANNEL_NONINTERACTIVE_PROVER_FELT_CHANNEL_H_
#define STARKWARE_CHANNEL_NONINTERACTIVE_PROVER_FELT_CHANNEL_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/crypt_tools/poseidon.h"

namespace starkware {

using FieldElementT = PrimeFieldElement<252, 0>;
using ValueType = FieldElementT::ValueType;
using HashT = Poseidon3;

class NoninteractiveProverFeltChannel : public ProverChannel {
 public:
  static constexpr size_t FeltSizeInBytes = FieldElementT::SizeInBytes();
  /*
    Initialize the channel's initial state to a value based on the public input and the constraints
    system. This ensures that the prover doesn't modify the public input after generating the proof.
  */
  explicit NoninteractiveProverFeltChannel(
      const FieldElementT initial_state, std::string pow_hash_name)
      : state_(initial_state), counter_(FieldElementT::Zero()), pow_hash_name_(pow_hash_name) {}

  /*
    SendBytes writes raw bytes to the proof and updates the hash chain.
    The felt channels require the number of bytes to be divisible by
    FeltSizeInBytes, and each FeltSizeInBytes chunk to be deserializable to felt.
  */
  void SendBytes(gsl::span<const std::byte> raw_bytes) override;

  /*
    Writes field elements to the proof, hashes these elements with the current state and updates it.
  */
  void SendFieldElementSpanImpl(const ConstFieldElementSpan& values) override;

  std::vector<std::byte> ReceiveBytes(size_t num_bytes) override {
    ASSERT_RELEASE(
        false, "Not implemented. Felt channel receives only felts via ReceiveFieldElement method.");
    return std::vector<std::byte>();
  };

  FieldElement ReceiveFieldElementImpl(const Field& field) override;

  /*
    Receives a random number from the verifier. The number should be chosen uniformly in the
    range [0, upper_bound).
  */
  uint64_t ReceiveNumberImpl(uint64_t upper_bound) override;

  /*
    This is done using ProofOfWork<pow_hash_name_>. Finds a nonce (8 bytes) for which
      H(H(magic || state || work_bits) || nonce)
    has security_bits leading zero bits. Then, pad the nonce with zeros to 32bytes and append it to
    the proof.
  */
  void ApplyProofOfWork(size_t security_bits) override;

  std::vector<std::byte> GetProof() const override;

 protected:
  std::vector<std::byte> GetStateBytesVector();

  // Internal function for sending field elements to the Verifier (assumes the elements were already
  // appended to the proof).
  void InnerSendFieldElements(gsl::span<const FieldElementT> elements);

 private:
  FieldElementT state_;
  FieldElementT counter_;
  std::vector<FieldElementT> proof_{};
  const std::string pow_hash_name_;
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_NONINTERACTIVE_PROVER_FELT_CHANNEL_H_
