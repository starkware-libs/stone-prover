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

#ifndef STARKWARE_COMMITMENT_SCHEME_MERKLE_MERKLE_COMMITMENT_SCHEME_H_
#define STARKWARE_COMMITMENT_SCHEME_MERKLE_MERKLE_COMMITMENT_SCHEME_H_

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/merkle/merkle.h"

namespace starkware {

template <typename HashT>
class MerkleCommitmentSchemeProver : public CommitmentSchemeProver {
 public:
  static constexpr size_t kMinSegmentBytes = 2 * HashT::kDigestNumBytes;
  static constexpr size_t kSizeOfElement = HashT::kDigestNumBytes;
  MerkleCommitmentSchemeProver(size_t n_elements, ProverChannel* channel);

  size_t NumSegments() const override;
  uint64_t SegmentLengthInElements() const override;
  size_t ElementLengthInBytes() const override { return kSizeOfElement; }

  void AddSegmentForCommitment(
      gsl::span<const std::byte> segment_data, size_t segment_index) override;

  void Commit() override;

  std::vector<uint64_t> StartDecommitmentPhase(const std::set<uint64_t>& queries) override;

  void Decommit(gsl::span<const std::byte> elements_data) override;

 private:
  const uint64_t n_elements_;
  ProverChannel* channel_;
  MerkleTree<HashT> tree_;
  std::set<uint64_t> queries_;
};

template <typename HashT>
class MerkleCommitmentSchemeVerifier : public CommitmentSchemeVerifier {
 public:
  MerkleCommitmentSchemeVerifier(uint64_t n_elements, VerifierChannel* channel);

  void ReadCommitment() override;
  bool VerifyIntegrity(
      const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify) override;

  uint64_t NumOfElements() const override { return n_elements_; };

 private:
  uint64_t n_elements_;
  VerifierChannel* channel_;
  std::optional<HashT> commitment_;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_MERKLE_MERKLE_COMMITMENT_SCHEME_H_
