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

#ifndef STARKWARE_CHANNEL_PROVER_CHANNEL_H_
#define STARKWARE_CHANNEL_PROVER_CHANNEL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/polymorphic/field.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/algebra/polymorphic/field_element_span.h"
#include "starkware/algebra/polymorphic/field_element_vector.h"
#include "starkware/channel/channel.h"
#include "starkware/channel/channel_statistics.h"
#include "starkware/crypt_tools/utils.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

using std::uint64_t;

class ProverChannel : public Channel {
 public:
  virtual ~ProverChannel() = default;

  void SendData(gsl::span<const std::byte> data, const std::string& annotation = "") {
    SendBytes(data);
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(annotation + ": Data(" + BytesToHexString(data) + ")", data.size());
    }
    proof_statistics_.data_count += 1;
  }

  void SendFieldElement(const FieldElement& value, const std::string& annotation = "") {
    SendFieldElementImpl(value);
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Field Element(" + value.ToString() + ")", value.SizeInBytes());
    }
    proof_statistics_.field_element_count += 1;
  }

  void SendFieldElementSpan(
      const ConstFieldElementSpan& values, const std::string& annotation = "") {
    SendFieldElementSpanImpl(values);
    if (AnnotationsEnabled()) {
      std::ostringstream oss;
      oss << annotation << ": Field Elements(" << values << ")";
      AnnotateProverToVerifier(oss.str(), values.Size() * values.GetField().ElementSizeInBytes());
    }
    proof_statistics_.field_element_count += values.Size();
  }

  template <typename HashT>
  void SendCommitmentHash(const HashT& hash, const std::string& annotation = "") {
    SendBytes(hash.GetDigest());
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Hash(" + hash.ToString() + ")", HashT::kDigestNumBytes);
    }
    proof_statistics_.commitment_count += 1;
    proof_statistics_.hash_count += 1;
  }

  FieldElement ReceiveFieldElement(const Field& field, const std::string& annotation = "") {
    FieldElement field_element = ReceiveFieldElementImpl(field);
    if (AnnotationsEnabled()) {
      AnnotateVerifierToProver(annotation + ": Field Element(" + field_element.ToString() + ")");
    }
    return field_element;
  }

  FieldElement GetRandomFieldElementFromVerifierImpl(
      const Field& field, const std::string& annotation) override {
    return ReceiveFieldElement(field, annotation);
  }

  template <typename HashT>
  void SendDecommitmentNode(const HashT& hash_node, const std::string& annotation = "") {
    SendBytes(hash_node.GetDigest());
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Hash(" + hash_node.ToString() + ")", HashT::kDigestNumBytes);
    }
    proof_statistics_.hash_count++;
  }

  /*
    Receives a random number from the verifier. The number should be chosen uniformly in the
    range [0, upper_bound).
  */
  uint64_t ReceiveNumber(uint64_t upper_bound, const std::string& annotation = "") {
    uint64_t number = ReceiveNumberImpl(upper_bound);
    if (AnnotationsEnabled()) {
      AnnotateVerifierToProver(annotation + ": Number(" + std::to_string(number) + ")");
    }
    return number;
  }

  uint64_t GetRandomNumberFromVerifierImpl(
      uint64_t upper_bound, const std::string& annotation) override {
    return ReceiveNumber(upper_bound, annotation);
  }

 protected:
  // ===============================================================================================
  // The following methods should not be overridden by the subclass, they are defined as virtual to
  // allow writing expectations on them using gmock.
  // ===============================================================================================
  virtual void SendFieldElementImpl(const FieldElement& value);
  virtual void SendFieldElementSpanImpl(const ConstFieldElementSpan& values);
  // ===============================================================================================
  // True pure virtual methods:
  // ===============================================================================================
  virtual FieldElement ReceiveFieldElementImpl(const Field& field) = 0;
  virtual uint64_t ReceiveNumberImpl(uint64_t upper_bound) = 0;
  virtual void SendBytes(gsl::span<const std::byte> raw_bytes) = 0;
  virtual std::vector<std::byte> ReceiveBytes(size_t num_bytes) = 0;
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_PROVER_CHANNEL_H_
