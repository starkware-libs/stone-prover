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

#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"

#include <algorithm>
#include <string>

#include "starkware/commitment_scheme/utils.h"
#include "starkware/crypt_tools/template_instantiation.h"
#include "starkware/crypt_tools/utils.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {

// Implementation of MerkleCommitmentScheme.

// Prover part.
template <typename HashT>
MerkleCommitmentSchemeProver<HashT>::MerkleCommitmentSchemeProver(
    size_t n_elements, ProverChannel* channel)
    : n_elements_(n_elements),
      channel_(channel),
      // Initialize the tree with the number of elements, each element is the hash stored in a leaf.
      tree_(MerkleTree<HashT>(n_elements_)) {}

template <typename HashT>
size_t MerkleCommitmentSchemeProver<HashT>::NumSegments() const {
  // Each segment contains one element.
  return n_elements_;
}

template <typename HashT>
uint64_t MerkleCommitmentSchemeProver<HashT>::SegmentLengthInElements() const {
  return 1;
}

template <typename HashT>
void MerkleCommitmentSchemeProver<HashT>::AddSegmentForCommitment(
    gsl::span<const std::byte> segment_data, size_t segment_index) {
  ASSERT_RELEASE(
      segment_data.size() == SegmentLengthInElements() * kSizeOfElement,
      "Segment size is " + std::to_string(segment_data.size()) + " instead of the expected " +
          std::to_string(kSizeOfElement * SegmentLengthInElements()));

  tree_.AddData(
      BytesAsHash<HashT>(segment_data, kSizeOfElement), segment_index * SegmentLengthInElements());
}

template <typename HashT>
void MerkleCommitmentSchemeProver<HashT>::Commit() {
  // After adding all segments, all inner tree nodes that are at least (tree_height -
  // log2(n_elements_in_segment_)) far from the root - were already computed.
  size_t tree_height = SafeLog2(tree_.k_data_length);
  const HashT commitment = tree_.GetRoot(tree_height - SafeLog2(SegmentLengthInElements()));
  channel_->SendCommitmentHash(commitment, "Commitment");
}

template <typename HashT>
std::vector<uint64_t> MerkleCommitmentSchemeProver<HashT>::StartDecommitmentPhase(
    const std::set<uint64_t>& queries) {
  queries_ = queries;
  return {};
}

template <typename HashT>
void MerkleCommitmentSchemeProver<HashT>::Decommit(gsl::span<const std::byte> elements_data) {
  ASSERT_RELEASE(elements_data.empty(), "element_data is expected to be empty");
  tree_.GenerateDecommitment(queries_, channel_);
}

// Verifier part.

template <typename HashT>
MerkleCommitmentSchemeVerifier<HashT>::MerkleCommitmentSchemeVerifier(
    uint64_t n_elements, VerifierChannel* channel)
    : n_elements_(n_elements), channel_(channel) {}

template <typename HashT>
void MerkleCommitmentSchemeVerifier<HashT>::ReadCommitment() {
  commitment_ = channel_->ReceiveCommitmentHash<HashT>("Commitment");
}

template <typename HashT>
bool MerkleCommitmentSchemeVerifier<HashT>::VerifyIntegrity(
    const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) {
  // Convert data to hashes.
  std::map<uint64_t, HashT> hashes_to_verify;

  for (auto const& element : elements_to_verify) {
    ASSERT_RELEASE(element.first < n_elements_, "Query out of range.");
    ASSERT_RELEASE(element.second.size() == HashT::kDigestNumBytes, "Element size mismatches.");
    hashes_to_verify[element.first] = HashT::InitDigestTo(element.second);
  }
  // Verify decommitment.
  return MerkleTree<HashT>::VerifyDecommitment(
      hashes_to_verify, n_elements_, *commitment_, channel_);
}

INSTANTIATE_FOR_ALL_HASH_FUNCTIONS(MerkleCommitmentSchemeProver);
INSTANTIATE_FOR_ALL_HASH_FUNCTIONS(MerkleCommitmentSchemeVerifier);

}  // namespace starkware
