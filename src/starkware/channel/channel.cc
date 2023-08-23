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

#include "starkware/channel/channel.h"

#include <string>
#include <vector>

#include "glog/logging.h"

namespace starkware {

void Channel::AnnotateProverToVerifier(const std::string& annotation, size_t n_bytes) {
  const size_t start = prover_to_verifier_bytes_;
  prover_to_verifier_bytes_ += n_bytes;
  const size_t end = prover_to_verifier_bytes_;

  AddAnnotation(
      "P->V[" + std::to_string(start) + ":" + std::to_string(end) + "]: " + annotation_prefix_ +
      annotation + "\n");
}

void Channel::AnnotateVerifierToProver(const std::string& annotation) {
  AddAnnotation("V->P: " + annotation_prefix_ + annotation + "\n");
}

bool Channel::AnnotationsEnabled() const { return annotations_enabled_; }
bool Channel::ExtraAnnotationsDisabled() const { return !extra_annotations_enabled_; }

std::ostream& operator<<(std::ostream& out, const Channel& channel) {
  out << "title " << channel.annotation_prefix_.substr(1, channel.annotation_prefix_.size() - 3)
      << " Proof Protocol\n\n";
  for (const std::string& s : channel.annotations_) {
    out << s;
  }
  out << "\nProof Statistics:\n\n";
  out << channel.proof_statistics_.ToString();
  return out;
}

void Channel::AddAnnotation(const std::string& annotation) {
  ASSERT_RELEASE(
      AnnotationsEnabled(),
      "Cannot add annotation when DisableAnnotations() was called. Check "
      "AnnotationsEnabled() before calling.");
  if (expected_annotations_) {
    const size_t idx = annotations_.size();
    ASSERT_RELEASE(idx < expected_annotations_->size(), "Expected annotations is too short.");
    const std::string& expected_annotation = expected_annotations_->at(idx);
    ASSERT_RELEASE(
        expected_annotation == annotation, "Annotation mismatch. Expected annotation: '" +
                                               expected_annotation + "'. Found: '" + annotation +
                                               "'");
  }
  annotations_.push_back(annotation);
}

}  // namespace starkware
