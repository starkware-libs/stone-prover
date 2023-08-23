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
#include <memory>
#include <utility>
#include <vector>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/commitment_scheme/commitment_scheme_mock.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::StrictMock;
using testing::Test;

/*
  Tests commitment and decommitment flows.
*/
TEST(CachingCommitmentSchemeProverTest, CommitAndDecommit) {
  // Preparing data for the test.
  Prng prng;
  const size_t n_segments = 2;
  const size_t size_of_element = prng.UniformInt<size_t>(1, 64);
  const size_t n_elements_in_segment = 4;
  const std::vector<std::byte> segment_0 =
      prng.RandomByteVector(size_of_element * n_elements_in_segment);
  const std::vector<std::byte> segment_1 =
      prng.RandomByteVector(size_of_element * n_elements_in_segment);

  auto inner_commitment_scheme = std::make_unique<StrictMock<CommitmentSchemeProverMock>>();
  // Get reference to inner_commitment_scheme before moving it to caching commitment scheme layer.
  StrictMock<CommitmentSchemeProverMock>* inner_scheme_ptr = inner_commitment_scheme.get();
  CachingCommitmentSchemeProver commitment_scheme_prover(
      size_of_element, n_elements_in_segment, n_segments, std::move(inner_commitment_scheme));

  // Commitment flow.
  EXPECT_CALL(*inner_scheme_ptr, AddSegmentForCommitment(gsl::span<const std::byte>(segment_0), 0));
  EXPECT_CALL(*inner_scheme_ptr, AddSegmentForCommitment(gsl::span<const std::byte>(segment_1), 1));
  EXPECT_CALL(*inner_scheme_ptr, Commit());

  commitment_scheme_prover.AddSegmentForCommitment(segment_1, 1);
  commitment_scheme_prover.AddSegmentForCommitment(segment_0, 0);
  commitment_scheme_prover.Commit();

  // Decommitment flow.
  const std::set<uint64_t> queries{1, 6};

  // Assumes inner layer packs 2 elements in each hash, hance for queries no. 1, 6 inner layer
  // needs elements no. 0, 1 for the first package and 6, 7 for the second package.
  std::vector<size_t> expected_indices_to_inner_layer{0, 1, 6, 7};
  std::vector<std::byte> queries_data;
  queries_data.reserve(expected_indices_to_inner_layer.size() * size_of_element);
  // Queries 0, 1 are the first 2 elements in segment_0. Queries 6,7 are the third and forth
  // elements in segment_1.
  std::copy_n(segment_0.begin(), 2 * size_of_element, std::back_inserter(queries_data));
  std::copy_n(
      segment_1.begin() + 2 * size_of_element, 2 * size_of_element,
      std::back_inserter(queries_data));

  // Expected decommitment flow.
  EXPECT_CALL(*inner_scheme_ptr, StartDecommitmentPhase(queries))
      .WillOnce(testing::Return(expected_indices_to_inner_layer));
  EXPECT_CALL(*inner_scheme_ptr, Decommit(gsl::span<const std::byte>(queries_data)));

  // Decommit.
  const auto res = commitment_scheme_prover.StartDecommitmentPhase(queries);
  const std::vector<uint64_t> expected_res = {};
  EXPECT_EQ(res, expected_res);
  commitment_scheme_prover.Decommit({});
}

}  // namespace
}  // namespace starkware
