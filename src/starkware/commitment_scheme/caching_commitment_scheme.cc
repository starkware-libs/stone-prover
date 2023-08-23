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

#include "starkware/commitment_scheme/caching_commitment_scheme.h"

#include <algorithm>
#include <string>
#include <utility>

#include "glog/logging.h"

#include "starkware/error_handling/error_handling.h"

namespace starkware {

// Implementation of cachingCommitmentScheme.

CachingCommitmentSchemeProver::CachingCommitmentSchemeProver(
    size_t size_of_element, uint64_t n_elements_in_segment, size_t n_segments,
    std::unique_ptr<CommitmentSchemeProver> inner_commitment_scheme)
    : size_of_element_(size_of_element),
      n_elements_in_segment_(n_elements_in_segment),
      n_segments_(n_segments),
      inner_commitment_scheme_(std::move(inner_commitment_scheme)),
      layer_data_(size_of_element_ * n_elements_in_segment_ * n_segments_) {}

size_t CachingCommitmentSchemeProver::GetSegmentOffsetInData(const size_t segment_index) {
  ASSERT_RELEASE(
      segment_index < NumSegments(), "Segment index: " + std::to_string(segment_index) +
                                         " is out of bound. There are only " +
                                         std::to_string(NumSegments()) + " segments.");
  return segment_index * SegmentLengthInBytes();
}

void CachingCommitmentSchemeProver::AddSegmentForCommitment(
    gsl::span<const std::byte> segment_data, size_t segment_index) {
  ASSERT_RELEASE(
      segment_data.size() == SegmentLengthInBytes(),
      "Segment data size: " + std::to_string(segment_data.size()) +
          "bytes is wrong. It should be: " + std::to_string(SegmentLengthInBytes()) + " bytes.");
  // Store segment data in memory and call the next layer with it.
  VLOG(5) << "Adding data for segment index " << segment_index << ", of size "
          << segment_data.size() << " bytes.";
  size_t segment_offset = GetSegmentOffsetInData(segment_index);
  std::copy(segment_data.begin(), segment_data.end(), layer_data_.begin() + segment_offset);

  inner_commitment_scheme_->AddSegmentForCommitment(segment_data, segment_index);
}

void CachingCommitmentSchemeProver::Commit() { inner_commitment_scheme_->Commit(); }

std::vector<uint64_t> CachingCommitmentSchemeProver::StartDecommitmentPhase(
    const std::set<uint64_t>& queries) {
  // Send required queries to inner_commitment_scheme_ and save the required queries needed for it.
  missing_element_queries_inner_layer_ = inner_commitment_scheme_->StartDecommitmentPhase(queries);
  // This commitment scheme layer doesn't need to get any data in order to decommit, because it
  // stores all the data it needs.
  return {};
}

void CachingCommitmentSchemeProver::Decommit(gsl::span<const std::byte> elements_data) {
  ASSERT_RELEASE(
      elements_data.empty(),
      "Caching commitment scheme doesn't need any information for its decommitment phase.");

  // Get the required data that was asked by the inner layer in StartDecommitmentPhase.
  const size_t inner_layer_data_size =
      missing_element_queries_inner_layer_.size() * size_of_element_;
  std::vector<std::byte> data_for_inner_layer;
  data_for_inner_layer.reserve(inner_layer_data_size);

  // Iterate over required elements and get their data.
  for (size_t element_index : missing_element_queries_inner_layer_) {
    auto data_query_start = element_index * size_of_element_;
    ASSERT_RELEASE(
        layer_data_.size() >= data_query_start + size_of_element_,
        "layer_data_ doesn't contain element #" + std::to_string(element_index));
    std::copy_n(
        layer_data_.begin() + data_query_start, size_of_element_,
        std::back_inserter(data_for_inner_layer));
  }

  ASSERT_RELEASE(
      data_for_inner_layer.size() == inner_layer_data_size,
      "Data size for inner layer doesn't fit the required data needed.");

  // Calls decommit of inner_commitment_scheme_ with the relevant data.
  inner_commitment_scheme_->Decommit(data_for_inner_layer);
}

}  // namespace starkware
