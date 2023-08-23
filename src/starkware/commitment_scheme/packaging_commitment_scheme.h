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

#ifndef STARKWARE_COMMITMENT_SCHEME_PACKAGING_COMMITMENT_SCHEME_H_
#define STARKWARE_COMMITMENT_SCHEME_PACKAGING_COMMITMENT_SCHEME_H_

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/packer_hasher.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

/*
  The factories are given as an input parameter to packaging commitment scheme prover and verifier
  (correspondingly) to enable creation of inner_commitment_scheme_ after creating the packer_. This
  is needed because packer_ calculates number of elements in packages which is needed to create
  inner_commitment_scheme_.
*/
using PackagingCommitmentSchemeProverFactory =
    std::function<std::unique_ptr<CommitmentSchemeProver>(size_t n_elements)>;

using PackagingCommitmentSchemeVerifierFactory =
    std::function<std::unique_ptr<CommitmentSchemeVerifier>(size_t n_elements)>;

/*
  One component in the flow of commit and decommit. In charge of packing elements in packages and
  communicate with the next component in the flow, which is stored as a member of the class
  inner_commitment_scheme_.
*/
template <typename HashT>
class PackagingCommitmentSchemeProver : public CommitmentSchemeProver {
 public:
  static constexpr size_t kMinSegmentBytes = 2 * HashT::kDigestNumBytes;
  PackagingCommitmentSchemeProver(
      size_t size_of_element, uint64_t n_elements_in_segment, size_t n_segments,
      ProverChannel* channel,
      const PackagingCommitmentSchemeProverFactory& inner_commitment_scheme_factory,
      bool is_merkle_layer = false);

  /*
    Constructs an instance of PackagingCommitmentSchemeProver for a Merkle layer.
    In particular, is_merkle_layer == true and each two elements are packed for the next layer.
  */
  PackagingCommitmentSchemeProver(
      size_t size_of_element, uint64_t n_elements_in_segment, size_t n_segments,
      ProverChannel* channel, std::unique_ptr<CommitmentSchemeProver> inner_commitment_scheme);

  size_t NumSegments() const override;
  size_t GetNumOfPackages() const;
  size_t ElementLengthInBytes() const override { return size_of_element_; }

  uint64_t SegmentLengthInElements() const override;

  /*
    Given a data segment, packes its elements in packages and hash each package. Call
    AddSegmentForCommitment of inner_commitment_scheme_ with the result.
  */
  void AddSegmentForCommitment(
      gsl::span<const std::byte> segment_data, size_t segment_index) override;

  /*
    Commit to data by calling commit of inner_commitment_scheme_.
  */
  void Commit() override;

  /*
    Start decommitment phase by calling inner_commitment_scheme_ with relevant  queries for
    decommitment. Returns the elements it needs in order to operate decommit (i.e., to compute
    hashes for the required queries).
  */
  std::vector<uint64_t> StartDecommitmentPhase(const std::set<uint64_t>& queries) override;

  void Decommit(gsl::span<const std::byte> elements_data) override;

 private:
  const size_t size_of_element_;
  const uint64_t n_elements_in_segment_;
  const size_t n_segments_;
  ProverChannel* channel_;
  const PackerHasher<HashT> packer_;
  std::unique_ptr<CommitmentSchemeProver> inner_commitment_scheme_;

  // Indicator for being a commitment scheme that does packaging and hashing of a merkle tree
  // layer. When set to true it saves memory - instead of storing the biggest layer of the merkle
  // tree in memory, it recomputes it on demand. Set to false by default.
  const bool is_merkle_layer_;

  std::set<uint64_t> queries_;

  // Indices of elements needed for the current commitment scheme to compute the required queries
  // given in queries_. Initialized in StartDecommitmentPhase.
  std::vector<uint64_t> missing_element_queries_;

  // Number of elements needed for inner_commitment_scheme to compute decommit. Initialzed with the
  // correct value in StartDecommitmentPhase.
  size_t n_missing_elements_for_inner_layer_ = 0;
};

/*
  Verifier's corresponding code of PackagingCommitmentSchemeProver.
*/
template <typename HashT>
class PackagingCommitmentSchemeVerifier : public CommitmentSchemeVerifier {
 public:
  PackagingCommitmentSchemeVerifier(
      size_t size_of_element, uint64_t n_elements, VerifierChannel* channel,
      const PackagingCommitmentSchemeVerifierFactory& inner_commitment_scheme_factory,
      bool is_merkle_layer);

  /*
    Constructs an instance of PackagingCommitmentSchemeVerifier for a Merkle layer.
    In particular, is_merkle_layer == true and each two elements are packed for the next layer.
  */
  PackagingCommitmentSchemeVerifier(
      size_t size_of_element, uint64_t n_elements, VerifierChannel* channel,
      std::unique_ptr<CommitmentSchemeVerifier> inner_commitment_scheme);

  /*
    Call ReadCommitment of inner_commitment_scheme_.
  */
  void ReadCommitment() override;

  /*
    Given elements_to_verify, verify elements using data it receives from the channel, and calls
    VerifyIntegrity of the inner layer.
  */
  bool VerifyIntegrity(
      const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) override;

  size_t GetNumOfPackages() const;

  uint64_t NumOfElements() const override { return n_elements_; }

  bool GetIsMerkleLayer() const { return is_merkle_layer_; }

 private:
  const size_t size_of_element_;
  const uint64_t n_elements_;
  VerifierChannel* channel_;
  const PackerHasher<HashT> packer_;
  std::unique_ptr<CommitmentSchemeVerifier> inner_commitment_scheme_;
  // See documentation for this param in PackagingCommitmentSchemeProver.
  const bool is_merkle_layer_;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_PACKAGING_COMMITMENT_SCHEME_H_
