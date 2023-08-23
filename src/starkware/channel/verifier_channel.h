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

#ifndef STARKWARE_CHANNEL_VERIFIER_CHANNEL_H_
#define STARKWARE_CHANNEL_VERIFIER_CHANNEL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <third_party/gsl/gsl-lite.hpp>

#include "starkware/algebra/polymorphic/field.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/algebra/polymorphic/field_element_span.h"
#include "starkware/algebra/polymorphic/field_element_vector.h"
#include "starkware/channel/channel.h"
#include "starkware/crypt_tools/utils.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

using std::uint64_t;

class VerifierChannel : public Channel {
 public:
  virtual ~VerifierChannel() = default;

  /*
    Generates a random number for the verifier, sends it to the prover and returns it. The number
    should be chosen uniformly in the range [0, upper_bound).
  */
  uint64_t GetAndSendRandomNumber(uint64_t upper_bound, const std::string& annotation = "") {
    uint64_t number = GetAndSendRandomNumberImpl(upper_bound);
    if (AnnotationsEnabled()) {
      AnnotateVerifierToProver(annotation + ": Number(" + std::to_string(number) + ")");
    }
    return number;
  }

  uint64_t GetRandomNumberFromVerifierImpl(
      uint64_t upper_bound, const std::string& annotation) override {
    return GetAndSendRandomNumber(upper_bound, annotation);
  }

  /*
    Generates a random field element from the requested field, sends it to the prover and returns
    it.
  */
  FieldElement GetAndSendRandomFieldElement(
      const Field& field, const std::string& annotation = "") {
    FieldElement field_element = GetAndSendRandomFieldElementImpl(field);
    if (AnnotationsEnabled()) {
      AnnotateVerifierToProver(annotation + ": Field Element(" + field_element.ToString() + ")");
    }
    return field_element;
  }

  FieldElement GetRandomFieldElementFromVerifierImpl(
      const Field& field, const std::string& annotation) override {
    return GetAndSendRandomFieldElement(field, annotation);
  }

  template <typename HashT>
  HashT ReceiveCommitmentHash(const std::string& annotation = "") {
    HashT hash = HashT::InitDigestTo(ReceiveBytes(HashT::kDigestNumBytes));
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Hash(" + hash.ToString() + ")", HashT::kDigestNumBytes);
    }
    proof_statistics_.commitment_count += 1;
    proof_statistics_.hash_count += 1;
    return hash;
  }

  FieldElement ReceiveFieldElement(const Field& field, const std::string& annotation = "") {
    FieldElement field_element = ReceiveFieldElementImpl(field);
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Field Element(" + field_element.ToString() + +")",
          field_element.SizeInBytes());
    }
    proof_statistics_.field_element_count += 1;
    return field_element;
  }

  void AnnotateExtraFieldElement(
      const FieldElement& field_element, const std::string& annotation = "") {
    AddExtraAnnotation(annotation + ": Field Element(" + field_element.ToString() + ")");
  }

  void ReceiveFieldElementSpan(
      const Field& field, const FieldElementSpan& span, const std::string& annotation = "") {
    ReceiveFieldElementSpanImpl(field, span);
    if (AnnotationsEnabled()) {
      std::ostringstream oss;
      oss << annotation << ": Field Elements(" << span << ")";
      AnnotateProverToVerifier(oss.str(), span.Size() * span.GetField().ElementSizeInBytes());
    }
    proof_statistics_.field_element_count += span.Size();
  }

  template <typename HashT>
  HashT ReceiveDecommitmentNode(const std::string& annotation = "") {
    HashT hash = HashT::InitDigestTo(ReceiveBytes(HashT::kDigestNumBytes));
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(
          annotation + ": Hash(" + hash.ToString() + ")", HashT::kDigestNumBytes);
    }

    proof_statistics_.hash_count++;

    return hash;
  }

  template <typename HashT>
  void AnnotateExtraDecommitmentNode(const HashT& hash, const std::string& annotation = "") {
    AddExtraAnnotation(annotation + ": Hash(" + hash.ToString() + ")");
  }

  std::vector<std::byte> ReceiveData(size_t num_bytes, const std::string& annotation = "") {
    const std::vector<std::byte> data = ReceiveBytes(num_bytes);
    if (AnnotationsEnabled()) {
      AnnotateProverToVerifier(annotation + ": Data(" + BytesToHexString(data) + ")", num_bytes);
    }
    proof_statistics_.data_count += 1;
    return data;
  }

  void DumpExtraAnnotations(std::ostream& out) {
    for (const std::string& s : extra_annotations_) {
      out << s;
    }
  }

 protected:
  // ===============================================================================================
  // The following methods should not be overridden by the subclass, they are defined as virtual to
  // allow writing expectations on them using gmock.
  // ===============================================================================================
  virtual uint64_t GetAndSendRandomNumberImpl(uint64_t upper_bound);
  virtual FieldElement GetAndSendRandomFieldElementImpl(const Field& field);
  virtual FieldElement ReceiveFieldElementImpl(const Field& field);
  virtual void ReceiveFieldElementSpanImpl(const Field& field, const FieldElementSpan& span);

  // ===============================================================================================
  // True pure virtual methods:
  // ===============================================================================================
  // Communication channel.
  virtual void SendBytes(gsl::span<const std::byte> raw_bytes) = 0;
  virtual std::vector<std::byte> ReceiveBytes(size_t num_bytes) = 0;
  // Randomness generator.
  virtual uint64_t GetRandomNumber(uint64_t upper_bound) = 0;
  virtual FieldElement GetRandomFieldElement(const Field& field) = 0;

 private:
  /*
    Send() functions should not be used by the verifier as they cannot be simulated by a
    non-interactive prover. However, internally the GetAndSendRandom*() functions use this API.
    This API is virtual only in order to allow for a gmock of verifier_channel.
  */
  virtual void SendNumber(uint64_t number);
  virtual void SendFieldElement(const FieldElement& value);

  std::vector<std::string> extra_annotations_{};

  void AddExtraAnnotation(const std::string& annotation) {
    extra_annotations_.push_back(annotation_prefix_ + annotation + "\n");
  }
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_VERIFIER_CHANNEL_H_
