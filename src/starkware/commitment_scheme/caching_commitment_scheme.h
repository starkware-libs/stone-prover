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

#ifndef STARKWARE_COMMITMENT_SCHEME_CACHING_COMMITMENT_SCHEME_H_
#define STARKWARE_COMMITMENT_SCHEME_CACHING_COMMITMENT_SCHEME_H_

#include <cstddef>
#include <memory>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/math/math.h"

namespace starkware {

/*
  One component in the flow of commit and decommit. Given a data to commit on, it saves the data to
  memory and calls the next commitment component.
  In more detail, we commit to the data in layers. This scheme saves elements of a single layer to
  the memory. When asked to decommit, this scheme takes the required elements from its storage.
  It communicates with the next component in the flow, which is stored as a member of the class
  inner_commitment_scheme_.
  To simulate an in-memory Merkle tree, one can use interleaved layers of packaging commitment
  scheme and caching commitment scheme (see packaging commitment scheme class for more info).
*/
class CachingCommitmentSchemeProver : public CommitmentSchemeProver {
 public:
  CachingCommitmentSchemeProver(
      size_t size_of_element, uint64_t n_elements_in_segment, size_t n_segments,
      std::unique_ptr<CommitmentSchemeProver> inner_commitment_scheme);

  size_t NumSegments() const override { return n_segments_; }
  size_t ElementLengthInBytes() const override { return size_of_element_; }

  uint64_t SegmentLengthInElements() const override { return n_elements_in_segment_; }

  uint64_t SegmentLengthInBytes() const { return n_elements_in_segment_ * size_of_element_; }

  void AddSegmentForCommitment(
      gsl::span<const std::byte> segment_data, size_t segment_index) override;

  void Commit() override;

  std::vector<uint64_t> StartDecommitmentPhase(const std::set<uint64_t>& queries) override;

  void Decommit(gsl::span<const std::byte> elements_data) override;

  /*
    Given segment index, returns its start location in layer_data_.
  */
  size_t GetSegmentOffsetInData(size_t segment_index);

 private:
  const size_t size_of_element_;
  const uint64_t n_elements_in_segment_;
  const size_t n_segments_;
  std::unique_ptr<CommitmentSchemeProver> inner_commitment_scheme_;

  /*
    Stores the elements of the current layer.
  */
  std::vector<std::byte> layer_data_;

  /*
    Indices of elements needed for the next commitment scheme to compute the required queries.
    Initialized in StartDecommitmentPhase.
  */
  std::vector<uint64_t> missing_element_queries_inner_layer_;
};

}  // namespace starkware

#endif  //  STARKWARE_COMMITMENT_SCHEME_CACHING_COMMITMENT_SCHEME_H_
