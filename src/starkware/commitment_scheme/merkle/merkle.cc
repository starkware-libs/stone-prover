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
#include <iterator>
#include <queue>

#include "glog/logging.h"

#include "starkware/commitment_scheme/merkle/merkle.h"
#include "starkware/crypt_tools/template_instantiation.h"
#include "starkware/crypt_tools/utils.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {

template <typename HashT>
void MerkleTree<HashT>::AddData(const gsl::span<const HashT>& data, uint64_t start_index) {
  ASSERT_DEBUG(
      start_index + data.size() <= k_data_length,
      "Data of length " + std::to_string(data.size()) + ", starting at " +
          std::to_string(start_index) + " exceeds the data length declared at tree construction, " +
          std::to_string(k_data_length) + ".");
  // Copy given data to the leaves of the tree.
  VLOG(5) << "Adding data at start_index = " << start_index << ", of size " << data.size();
  std::copy(data.begin(), data.end(), nodes_.begin() + k_data_length + start_index);
  // Hash to compute all internal nodes that can be derived solely from the given data.
  uint64_t cur = (k_data_length + start_index) / 2;
  // Based on the given data, we compute its parent nodes' hashes (referred to here as "sub_layer").
  for (size_t sub_layer_length = data.size() / 2; sub_layer_length > 0;
       sub_layer_length /= 2, cur /= 2) {
    for (size_t i = cur; i < cur + sub_layer_length; i++) {
      // Compute next sub-layer.
      nodes_[i] = HashT::Hash(nodes_[i * 2], nodes_[i * 2 + 1]);
      VLOG(6) << "Wrote to inner node #" << i;
    }
  }
}

template <typename HashT>
HashT MerkleTree<HashT>::GetRoot(size_t min_depth_assumed_correct) {
  // Iterating nodes in reverse order to traverse up the tree layer by layer.
  VLOG(4) << "Computing root, assuming correctness of nodes at depth " << min_depth_assumed_correct;
  ASSERT_RELEASE(
      min_depth_assumed_correct < SafeLog2(nodes_.size()),
      "Depth assumed correct must be at most the tree's height.");
  for (uint64_t i = Pow2(min_depth_assumed_correct) - 1; i > 0; i--) {
    nodes_[i] = HashT::Hash(nodes_[i * 2], nodes_[i * 2 + 1]);
  }
  return nodes_[1];
}

template <typename HashT>
void MerkleTree<HashT>::GenerateDecommitment(
    const std::set<uint64_t>& queries, ProverChannel* channel) const {
  ASSERT_RELEASE(!queries.empty(), "Empty input queries.");

  std::queue<uint64_t> queue;

  // Initialize the queue with the query leaves.
  // Fix offset (the user of the function gives queries w.r.t. the data, we use them as indices of
  // the tree's leaves).
  for (auto query_idx : queries) {
    ASSERT_RELEASE(query_idx < k_data_length, "Query out of range.");
    queue.push(query_idx + k_data_length);
  }

  uint64_t node_index = queue.front();
  // Iterate over the queue until we reach the root node.
  while (node_index != uint64_t(1)) {
    queue.pop();

    // Add the parent node to the queue, before sibling check to avoid empty queue.
    queue.push(node_index / 2);
    uint64_t sibling_node_index = node_index ^ 1;
    if (queue.front() == sibling_node_index) {
      // Next node is the sibling - Need to skip it.
      queue.pop();
    } else {
      // Next node is not the sibling - Add the sibling to the decommitment.
      SendDecommitmentNode(sibling_node_index, channel);
    }

    node_index = queue.front();
  }
}

template <typename HashT>
void MerkleTree<HashT>::SendDecommitmentNode(uint64_t node_index, ProverChannel* channel) const {
  channel->SendDecommitmentNode(nodes_[node_index], "For node " + std::to_string(node_index));
}

template <typename HashT>
bool MerkleTree<HashT>::VerifyDecommitment(
    const std::map<uint64_t, HashT>& data_to_verify, uint64_t total_data_length,
    const HashT& merkle_root, VerifierChannel* channel) {
  ASSERT_VERIFIER(
      total_data_length > 0, "Data length has to be at least 1 (i.e. tree cannot be empty).");

  std::queue<std::pair<uint64_t, HashT>> queue;
  // Fix offset of query enumeration.
  for (const auto& to_verify : data_to_verify) {
    queue.emplace(to_verify.first + total_data_length, to_verify.second);
  }

  // We iterate over the known nodes, i.e. the ones given within data_to_verify or computed from
  // known nodes, and using the decommitment nodes - we add more 'known nodes' to the pool, until
  // either we have no more known nodes, or we can compute the hash of the root.
  std::array<HashT, 2> siblings = {};

  uint64_t node_index;
  HashT node_hash;
  std::tie(node_index, node_hash) = queue.front();
  while (node_index != uint64_t(1)) {
    queue.pop();
    gsl::at(siblings, node_index & 1) = node_hash;

    HashT sibling_node_hash;
    uint64_t sibling_node_index = node_index ^ 1;
    if (!queue.empty() && queue.front().first == sibling_node_index) {
      // Node's sibling is already known. Take it from known_nodes.
      VLOG(7) << "Node " << node_index << "'s sibling is already known.";
      sibling_node_hash = queue.front().second;
      queue.pop();
    } else {
      // This node's sibling is part of the authentication nodes. Read it from the channel.
      const HashT decommitment_node =
          channel->ReceiveDecommitmentNode<HashT>("For node " + std::to_string(sibling_node_index));
      VLOG(7) << "Fetching node " << sibling_node_index << " from channel";
      sibling_node_hash = decommitment_node;
    }
    gsl::at(siblings, sibling_node_index & 1) = sibling_node_hash;
    VLOG(7) << "Adding hash for " << node_index;
    VLOG(7) << "Hashing " << siblings[0] << " and " << siblings[1];
    queue.emplace(node_index / 2, HashT::Hash(siblings[0], siblings[1]));

    std::tie(node_index, node_hash) = queue.front();
  }

  return queue.front().second == merkle_root;
}

INSTANTIATE_FOR_ALL_HASH_FUNCTIONS(MerkleTree);

}  // namespace starkware
