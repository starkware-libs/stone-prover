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

#ifndef STARKWARE_CHANNEL_NONINTERACTIVE_VERIFIER_FELT_CHANNEL_H_
#define STARKWARE_CHANNEL_NONINTERACTIVE_VERIFIER_FELT_CHANNEL_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/crypt_tools/poseidon.h"

namespace starkware {

using FieldElementT = PrimeFieldElement<252, 0>;
using ValueType = FieldElementT::ValueType;
using HashT = Poseidon3;

class NoninteractiveVerifierFeltChannel : public VerifierChannel {
 public:
  static constexpr size_t FeltSizeInBytes = FieldElementT::SizeInBytes();
  /*
    Initialize the channel's initial state to a value based on the public input and the constraints
    system. This ensures that the prover doesn't modify the public input after generating the proof.
  */
  explicit NoninteractiveVerifierFeltChannel(
      const FieldElementT initial_state, gsl::span<const std::byte> proof,
      std::string pow_hash_name)
      : state_(initial_state), counter_(FieldElementT::Zero()), pow_hash_name_(pow_hash_name) {
    ASSERT_RELEASE(proof.size() % FeltSizeInBytes == 0, "Bad input legnth.");
    proof_.reserve(proof.size());
    for (size_t felt_idx = 0; felt_idx < proof.size(); felt_idx += FeltSizeInBytes) {
      // Read the field elements from the proofs and check they are in range with the field.
      ValueType element =
          FieldElementT::ValueType::FromBytes(proof.subspan(felt_idx, FeltSizeInBytes));
      ASSERT_RELEASE(
          element < FieldElementT::GetModulus(), "The input must be smaller than the field prime.");
      proof_.push_back(FieldElementT::FromBigInt(element));
    }
  }

  /*
    SendBytes does not send anything.
  */
  void SendBytes(gsl::span<const std::byte> raw_bytes) override;

  /*
    ReceiveBytes reads felts from the proof and updates the state accordingly.
  */
  std::vector<std::byte> ReceiveBytes(size_t num_bytes) override;

  /*
    Reads field elements from the proof, hashes these elements with the state and updates it.
  */
  void ReceiveFieldElementSpanImpl(const Field& field, const FieldElementSpan& span) override;

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

 protected:
  std::vector<std::byte> GetStateBytesVector();

  // Internal function for receiving field elements from the prover.
  gsl::span<const FieldElementT> ReceiveFieldElements(size_t n_felts);

 private:
  FieldElementT state_;
  FieldElementT counter_;
  std::vector<FieldElementT> proof_{};
  size_t proof_read_index_ = 0;
  const std::string pow_hash_name_;
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_NONINTERACTIVE_VERIFIER_FELT_CHANNEL_H_
