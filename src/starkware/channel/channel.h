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

#ifndef STARKWARE_CHANNEL_CHANNEL_H_
#define STARKWARE_CHANNEL_CHANNEL_H_

#include <optional>
#include <string>
#include <vector>

#include "starkware/algebra/polymorphic/field.h"
#include "starkware/channel/channel_statistics.h"

namespace starkware {

class Channel {
 public:
  FieldElement GetRandomFieldElementFromVerifier(
      const Field& field, const std::string& annotation = "") {
    return GetRandomFieldElementFromVerifierImpl(field, annotation);
  }
  virtual FieldElement GetRandomFieldElementFromVerifierImpl(
      const Field& field, const std::string& annotation) = 0;

  uint64_t GetRandomNumberFromVerifier(uint64_t upper_bound, const std::string& annotation = "") {
    return GetRandomNumberFromVerifierImpl(upper_bound, annotation);
  }
  virtual uint64_t GetRandomNumberFromVerifierImpl(
      uint64_t upper_bound, const std::string& annotation) = 0;

  /*
    Only relevant for non-interactive channels. Changes the channel seed to a "safer" seed.

    This function guarantees that randomness fetched from the channel after calling this function
    and before sending data from the prover to the channel, is "safe": A malicious
    prover will have to perform 2^security_bits operations for each attempt to randomize the fetched
    randomness.

    Increases the amount of work a malicious prover needs to perform, in order to fake a proof.
  */
  virtual void ApplyProofOfWork(size_t security_bits) = 0;

  void EnterAnnotationScope(const std::string& scope) {
    annotation_scope_.push_back(scope);
    UpdateAnnotationPrefix();
  }
  void ExitAnnotationScope() {
    annotation_scope_.pop_back();
    UpdateAnnotationPrefix();
  }
  void DisableAnnotations() { annotations_enabled_ = false; }
  void DisableExtraAnnotations() { extra_annotations_enabled_ = false; }
  bool ExtraAnnotationsDisabled() const;

  const ChannelStatistics& GetStatistics() const { return proof_statistics_; }

  /*
    Sets a vector of expected annotations. The Channel will check that the annotations it
    generates, match the annotations in this vector. Usually, this vector is the annotations created
    by the prover channel).
  */
  void SetExpectedAnnotations(const std::vector<std::string>& expected_annotations) {
    expected_annotations_ = expected_annotations;
  }

  const std::vector<std::string>& GetAnnotations() const { return annotations_; }

  /*
    This function is called after the Verifier finished sending randomness to the Prover, and
    doesn't let the verify send randomness after it is called.
  */
  void BeginQueryPhase() { in_query_phase_ = true; }

 protected:
  /*
    Adds an annotation for information sent from the prover to the verifier.
  */
  void AnnotateProverToVerifier(const std::string& annotation, size_t n_bytes);

  /*
    Adds an annotation for information sent from the verifier to the prover.
  */
  void AnnotateVerifierToProver(const std::string& annotation);

  friend std::ostream& operator<<(std::ostream& out, const Channel& channel);

  std::string annotation_prefix_ = ": ";
  ChannelStatistics proof_statistics_;
  bool AnnotationsEnabled() const;
  bool in_query_phase_ = false;

 private:
  void AddAnnotation(const std::string& annotation);

  /*
    Call this function every time that the annotation scope is updated to recalculate the prefix to
    be added to annotations. It takes all annotation scopes in the annotation_scope_ vector and
    concatenates them with "/" delimiters.
  */
  void UpdateAnnotationPrefix() {
    annotation_prefix_ = "";
    for (const std::string& annotation : annotation_scope_) {
      annotation_prefix_ += ("/" + annotation);
    }
    annotation_prefix_ += ": ";
  }
  std::vector<std::string> annotation_scope_{};
  std::vector<std::string> annotations_{};
  bool annotations_enabled_ = true;
  bool extra_annotations_enabled_ = true;
  size_t prover_to_verifier_bytes_ = 0;

  std::optional<std::vector<std::string>> expected_annotations_ = std::nullopt;
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_CHANNEL_H_
