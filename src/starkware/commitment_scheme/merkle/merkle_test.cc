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

#include <algorithm>
#include <cstddef>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/commitment_scheme/merkle/merkle.h"
#include "starkware/crypt_tools/blake2s.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {
namespace {

template <typename HashT>
class MerkleTreeTest : public ::testing::Test {};

using TestedHashTypes = ::testing::Types<Blake2s256>;

TYPED_TEST_CASE(MerkleTreeTest, TestedHashTypes);

// Auxiliary function to generate random hashes for the data given to the tree.
template <typename HashT>
std::vector<HashT> GetRandomData(uint64_t length, Prng* prng) {
  std::vector<HashT> data;
  data.reserve(length);
  for (uint64_t i = 0; i < length; ++i) {
    std::vector<std::byte> digest;
    data.push_back(HashT::InitDigestTo(prng->RandomByteVector(HashT::kDigestNumBytes)));
  }
  return data;
}

// Compute root twice, make sure we're consistent.
TYPED_TEST(MerkleTreeTest, ComputeRootTwice) {
  Prng prng;
  size_t tree_height = prng.UniformInt(0, 10);
  std::vector<TypeParam> data = GetRandomData<TypeParam>(Pow2(tree_height), &prng);
  MerkleTree<TypeParam> tree(data.size());
  tree.AddData(data, 0);
  TypeParam root1 = tree.GetRoot(tree_height);
  TypeParam root2 = tree.GetRoot(tree_height);
  EXPECT_EQ(root1, root2);
}

// Check that starting computation from different depths gets the same value.
TYPED_TEST(MerkleTreeTest, GetRootFromDifferentDepths) {
  Prng prng;
  size_t tree_height = prng.UniformInt(1, 10);
  // Just to make things interesting - we don't feed the data in one go, but in two segments.
  std::vector<TypeParam> data = GetRandomData<TypeParam>(Pow2(tree_height - 1), &prng);
  MerkleTree<TypeParam> tree(data.size() * 2);
  tree.AddData(data, 0);
  tree.AddData(data, data.size());
  for (size_t i = 0; i < 20; ++i) {
    TypeParam root1 = tree.GetRoot(prng.UniformInt<size_t>(1, tree_height));
    TypeParam root2 = tree.GetRoot(prng.UniformInt<size_t>(1, tree_height));
    EXPECT_EQ(root1, root2);
  }
}

// Check that different trees get different roots.
TYPED_TEST(MerkleTreeTest, DifferentRootForDifferentTrees) {
  Prng prng;
  size_t tree_height = prng.UniformInt(0, 10);
  std::vector<TypeParam> data = GetRandomData<TypeParam>(Pow2(tree_height), &prng);
  MerkleTree<TypeParam> tree(data.size());
  tree.AddData(data, 0);
  TypeParam root1 = tree.GetRoot(0);
  tree.AddData(GetRandomData<TypeParam>(1, &prng), 0);
  TypeParam root2 = tree.GetRoot(tree_height);
  EXPECT_NE(root1, root2);
}

/*
  Generate random queries to a random tree, check that the decommitment passes verification.
*/
TYPED_TEST(MerkleTreeTest, QueryVerificationPositive) {
  Prng prng;
  uint64_t data_length = Pow2(prng.UniformInt(0, 10));
  std::vector<TypeParam> data = GetRandomData<TypeParam>(data_length, &prng);
  MerkleTree<TypeParam> tree(data.size());
  tree.AddData(data, 0);
  TypeParam root = tree.GetRoot(0);
  std::set<uint64_t> queries;
  size_t num_queries = prng.UniformInt(static_cast<size_t>(1), std::min<size_t>(10, data_length));
  std::map<uint64_t, TypeParam> query_data;
  while (queries.size() < num_queries) {
    uint64_t query = prng.UniformInt(static_cast<uint64_t>(0), data_length - 1);
    queries.insert(query);
    query_data[query] = data[query];
  }

  const Prng channel_prng;
  NoninteractiveProverChannel prover_channel(channel_prng.Clone());
  VLOG(4) << "Testing " << num_queries << " queries to a tree with " << data_length << " leaves.";
  tree.GenerateDecommitment(queries, &prover_channel);
  VLOG(4) << "Queries are : " << queries;
  VLOG(4) << "Data is : " << query_data;
  VLOG(4) << "Root is : " << root;
  NoninteractiveVerifierChannel verifier_channel(channel_prng.Clone(), prover_channel.GetProof());
  EXPECT_TRUE(
      MerkleTree<TypeParam>::VerifyDecommitment(query_data, data_length, root, &verifier_channel));
}

/*
  Generate random queries to a random tree, get decommitment, and then try to verify the
  decommitment with data that differs a bit from the one initially given to the tree. We expect it
  to fail.
*/
TYPED_TEST(MerkleTreeTest, QueryVerificationNegative) {
  Prng prng;
  uint64_t data_length = Pow2(prng.UniformInt(0, 10));
  std::vector<TypeParam> data = GetRandomData<TypeParam>(data_length, &prng);
  MerkleTree<TypeParam> tree(data.size());
  tree.AddData(data, 0);
  TypeParam root = tree.GetRoot(0);
  std::set<uint64_t> queries;
  size_t num_queries =
      prng.UniformInt(static_cast<uint64_t>(1), std::min<uint64_t>(10, data_length));
  std::map<uint64_t, TypeParam> query_data;
  while (queries.size() < num_queries) {
    uint64_t query = prng.UniformInt(static_cast<uint64_t>(0), data_length - 1);
    queries.insert(query);
    query_data[query] = data[query];
  }
  // Change one item in the query data.
  auto it = query_data.begin();
  std::advance(it, prng.template UniformInt<size_t>(0, query_data.size() - 1));
  it->second = GetRandomData<TypeParam>(1, &prng)[0];
  VLOG(3) << "Testing " << num_queries << " queries to a tree with " << data_length << " leaves.";

  const Prng channel_prng;
  NoninteractiveProverChannel prover_channel(channel_prng.Clone());
  tree.GenerateDecommitment(queries, &prover_channel);
  NoninteractiveVerifierChannel verifier_channel(channel_prng.Clone(), prover_channel.GetProof());
  EXPECT_FALSE(
      MerkleTree<TypeParam>::VerifyDecommitment(query_data, data_length, root, &verifier_channel));
}

}  // namespace
}  // namespace starkware
