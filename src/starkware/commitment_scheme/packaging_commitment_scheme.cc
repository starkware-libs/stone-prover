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

#include "starkware/commitment_scheme/packaging_commitment_scheme.h"

#include <algorithm>
#include <string>
#include <utility>

#include "starkware/commitment_scheme/packer_hasher.h"
#include "starkware/crypt_tools/template_instantiation.h"
#include "starkware/crypt_tools/utils.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {

// Implementation of packagingCommitmentScheme.

template <typename HashT>
PackagingCommitmentSchemeProver<HashT>::PackagingCommitmentSchemeProver(
    size_t size_of_element, uint64_t n_elements_in_segment, size_t n_segments,
    ProverChannel* channel,
    const PackagingCommitmentSchemeProverFactory& inner_commitment_scheme_factory,
    bool is_merkle_layer)
    : size_of_element_(size_of_element),
      n_elements_in_segment_(n_elements_in_segment),
      n_segments_(n_segments),
      channel_(channel),
      packer_(PackerHasher<HashT>(size_of_element_, n_segments_ * n_elements_in_segment_)),
      inner_commitment_scheme_(inner_commitment_scheme_factory(packer_.k_n_packages)),
      is_merkle_layer_(is_merkle_layer),
      missing_element_queries_({}) {
  if (is_merkle_layer_) {
    ASSERT_RELEASE(
        packer_.k_n_elements_in_package == 2,
        "Wrong number of elements in initialization of packaging commitment scheme: " +
            std::to_string(packer_.k_n_elements_in_package));
  }
}

template <typename HashT>  // NOLINT clang-tidy bug, delegating constructor in templated class.
PackagingCommitmentSchemeProver<HashT>::PackagingCommitmentSchemeProver(
    size_t size_of_element, uint64_t n_elements_in_segment, size_t n_segments,
    ProverChannel* channel, std::unique_ptr<CommitmentSchemeProver> inner_commitment_scheme)
    : PackagingCommitmentSchemeProver(
          size_of_element, n_elements_in_segment, n_segments, channel,
          [&inner_commitment_scheme](size_t /*n_elements*/) {
            return std::move(inner_commitment_scheme);
          },
          true) {
  ASSERT_RELEASE(
      2 * inner_commitment_scheme_->SegmentLengthInElements() == n_elements_in_segment_,
      "Expected a ratio of 2 between n_elements_in_segment in current layer and next layer. In the "
      "current layer: " +
          std::to_string(n_elements_in_segment_) + ", in next layer: " +
          std::to_string(inner_commitment_scheme_->SegmentLengthInElements()));
}

template <typename HashT>
size_t PackagingCommitmentSchemeProver<HashT>::NumSegments() const {
  return n_segments_;
}

template <typename HashT>
size_t PackagingCommitmentSchemeProver<HashT>::GetNumOfPackages() const {
  return packer_.k_n_packages;
}

template <typename HashT>
uint64_t PackagingCommitmentSchemeProver<HashT>::SegmentLengthInElements() const {
  return n_elements_in_segment_;
}

template <typename HashT>
void PackagingCommitmentSchemeProver<HashT>::AddSegmentForCommitment(
    gsl::span<const std::byte> segment_data, size_t segment_index) {
  ASSERT_RELEASE(
      segment_data.size() == n_elements_in_segment_ * size_of_element_,
      "Segment size is " + std::to_string(segment_data.size()) + " instead of the expected " +
          std::to_string(size_of_element_ * n_elements_in_segment_));
  ASSERT_RELEASE(
      segment_index < NumSegments(),
      "Segment index " + std::to_string(segment_index) +
          " is out of range. There are: " + std::to_string(NumSegments()) + " segments.");
  inner_commitment_scheme_->AddSegmentForCommitment(
      packer_.PackAndHash(segment_data, is_merkle_layer_), segment_index);
}

template <typename HashT>
void PackagingCommitmentSchemeProver<HashT>::Commit() {
  inner_commitment_scheme_->Commit();
}

template <typename HashT>
std::vector<uint64_t> PackagingCommitmentSchemeProver<HashT>::StartDecommitmentPhase(
    const std::set<uint64_t>& queries) {
  queries_ = queries;
  // Compute missing elements required to compute hashes for current layer.
  missing_element_queries_ = packer_.ElementsRequiredToComputeHashes(queries_);

  // Translate query indices from element indices to package indices, since this is what
  // inner_commitment_scheme handles.
  std::set<uint64_t> package_queries_to_inner_layer;
  for (uint64_t q : queries_) {
    package_queries_to_inner_layer.insert(q / packer_.k_n_elements_in_package);
  }
  // Send required queries to inner_commitment_scheme_ and get required queries needed for it.
  auto missing_package_queries_inner_layer =
      inner_commitment_scheme_->StartDecommitmentPhase(package_queries_to_inner_layer);
  // Translate inner layer queries to current layer requests for element.
  const auto missing_element_queries_to_inner_layer =
      packer_.GetElementsInPackages(missing_package_queries_inner_layer);

  n_missing_elements_for_inner_layer_ = missing_element_queries_to_inner_layer.size();
  std::vector<uint64_t> all_missing_elements;
  all_missing_elements.reserve(
      missing_element_queries_.size() + n_missing_elements_for_inner_layer_);
  // missing_element_queries and missing_element_queries_to_inner_layer are disjoint sets.
  all_missing_elements.insert(
      all_missing_elements.end(), missing_element_queries_.begin(), missing_element_queries_.end());
  all_missing_elements.insert(
      all_missing_elements.end(), missing_element_queries_to_inner_layer.begin(),
      missing_element_queries_to_inner_layer.end());

  return all_missing_elements;
}

template <typename HashT>
void PackagingCommitmentSchemeProver<HashT>::Decommit(gsl::span<const std::byte> elements_data) {
  ASSERT_RELEASE(
      elements_data.size() == size_of_element_ * (missing_element_queries_.size() +
                                                  n_missing_elements_for_inner_layer_),
      "Data size of data given in Decommit doesn't fit request in StartDecommitmentPhase.");

  // Send to channel the elements current packaging commitment scheme got according to its request
  // in StartDecommitmentPhase.
  for (size_t i = 0; i < missing_element_queries_.size(); i++) {
    const auto bytes_to_send = elements_data.subspan(i * size_of_element_, size_of_element_);
    if (is_merkle_layer_) {
      // Send decommitment node with its index in the full merkle. The index is calculated as
      // follows: 2 * GetNumOfPackages() + missing_element_queries_[i].
      channel_->SendDecommitmentNode(
          HashT::InitDigestTo(bytes_to_send),
          "For node " + std::to_string(2 * GetNumOfPackages() + missing_element_queries_[i]));
    } else {
      channel_->SendData(
          bytes_to_send,
          "To complete packages, element #" + std::to_string(missing_element_queries_[i]));
    }
  }

  // Pack and hash the data inner_commitment_scheme_ requested in StartDecommitmentPhase and send
  // it to inner_commitment_scheme_.
  std::vector<std::byte> data_for_inner_layer = packer_.PackAndHash(
      elements_data.subspan(
          missing_element_queries_.size() * size_of_element_,
          n_missing_elements_for_inner_layer_ * size_of_element_),
      is_merkle_layer_);
  inner_commitment_scheme_->Decommit(data_for_inner_layer);
}

// Verifier part.

template <typename HashT>
PackagingCommitmentSchemeVerifier<HashT>::PackagingCommitmentSchemeVerifier(
    size_t size_of_element, uint64_t n_elements, VerifierChannel* channel,
    const PackagingCommitmentSchemeVerifierFactory& inner_commitment_scheme_factory,
    bool is_merkle_layer)
    : size_of_element_(size_of_element),
      n_elements_(n_elements),
      channel_(channel),
      packer_(PackerHasher<HashT>(size_of_element, n_elements_)),
      inner_commitment_scheme_(inner_commitment_scheme_factory(packer_.k_n_packages)),
      is_merkle_layer_(is_merkle_layer) {
  if (is_merkle_layer_) {
    ASSERT_RELEASE(
        packer_.k_n_elements_in_package == 2,
        "Wrong number of elements in initialization of packaging commitment scheme: " +
            std::to_string(packer_.k_n_elements_in_package));
  }
}

template <typename HashT>  // NOLINT clang-tidy bug, delegating constructor in templated class.
PackagingCommitmentSchemeVerifier<HashT>::PackagingCommitmentSchemeVerifier(
    size_t size_of_element, uint64_t n_elements, VerifierChannel* channel,
    std::unique_ptr<CommitmentSchemeVerifier> inner_commitment_scheme)
    : PackagingCommitmentSchemeVerifier(
          size_of_element, n_elements, channel,
          [&inner_commitment_scheme](size_t /*n_elements*/) {
            return std::move(inner_commitment_scheme);
          },
          true) {
  ASSERT_RELEASE(
      2 * inner_commitment_scheme_->NumOfElements() == n_elements_,
      "Expected a ratio of 2 between n_elements in current layer and next layer. In the "
      "current layer: " +
          std::to_string(n_elements_) +
          ", in next layer: " + std::to_string(inner_commitment_scheme_->NumOfElements()));
}

template <typename HashT>
void PackagingCommitmentSchemeVerifier<HashT>::ReadCommitment() {
  inner_commitment_scheme_->ReadCommitment();
}

template <typename HashT>
bool PackagingCommitmentSchemeVerifier<HashT>::VerifyIntegrity(
    const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) {
  // Get missing elements (i.e., ones in the same packages as at least one elements_to_verify, but
  // that are not elements that the verifier actually asked about) by reading from decommitment.
  // For example - if elements_to_verify equals to {2,8} and there are 4 elements in each package
  // then missing_elements_idxs = {0,1,3,9,10,11}: 0,1,3 to verify the package for element 2 and
  // 9,10,11 to verify the package for element 8.
  const std::vector<uint64_t> missing_elements_idxs =
      packer_.ElementsRequiredToComputeHashes(Keys(elements_to_verify));

  std::map<uint64_t, std::vector<std::byte>> full_data_to_verify(elements_to_verify);
  for (const uint64_t missing_element_idx : missing_elements_idxs) {
    if (is_merkle_layer_) {
      const std::string msg =
          "For node " + std::to_string(2 * GetNumOfPackages() + missing_element_idx);
      const auto result_array = channel_->ReceiveDecommitmentNode<HashT>(msg).GetDigest();
      full_data_to_verify[missing_element_idx] =
          std::vector<std::byte>(std::begin(result_array), std::end(result_array));
    } else {
      full_data_to_verify[missing_element_idx] = channel_->ReceiveData(
          size_of_element_,
          "To complete packages, element #" + std::to_string(missing_element_idx));
    }
  }

  // Convert data to bytes.
  const std::map<uint64_t, std::vector<std::byte>> bytes_to_verify =
      packer_.PackAndHash(full_data_to_verify, is_merkle_layer_);

  if (!is_merkle_layer_) {
    for (auto const& element : bytes_to_verify) {
      channel_->AnnotateExtraDecommitmentNode<HashT>(
          HashT::InitDigestTo(element.second),
          "For node " + std::to_string(element.first + inner_commitment_scheme_->NumOfElements()));
    }
  }

  return inner_commitment_scheme_->VerifyIntegrity(bytes_to_verify);
}

template <typename HashT>
size_t PackagingCommitmentSchemeVerifier<HashT>::GetNumOfPackages() const {
  return packer_.k_n_packages;
}

INSTANTIATE_FOR_ALL_HASH_FUNCTIONS(PackagingCommitmentSchemeProver);
INSTANTIATE_FOR_ALL_HASH_FUNCTIONS(PackagingCommitmentSchemeVerifier);

}  // namespace starkware
