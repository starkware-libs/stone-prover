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

#ifndef STARKWARE_COMMITMENT_SCHEME_MERKLE_MERKLE_H_
#define STARKWARE_COMMITMENT_SCHEME_MERKLE_MERKLE_H_

#include <array>
#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"

namespace starkware {

/*
  Merkle Tree.
*/
template <typename HashT>
class MerkleTree {
 public:
  explicit MerkleTree(uint64_t data_length)
      : k_data_length(data_length), nodes_(2 * k_data_length) {
    ASSERT_RELEASE(IsPowerOfTwo(data_length), "Data length is not a power of 2!");
    VLOG(3) << "Constructing a Merkle tree for data length = " << data_length;
    // We use an array that has one extra cell we never use at the beginning, to make indexing
    // nicer.
  }

  /*
    Methods to feed the tree with data to commit on. The start_index is used so that data may be fed
    into the tree in any order, and by different threads.
    start_index + data.size() has to be smaller than the data length declared at construction.
  */
  void AddData(const gsl::span<const HashT>& data, uint64_t start_index);

  /*
    Retrieves the root of the tree.
    This entails computing inner-nodes hashes, however, some of the inner nodes' hashes may already
    be known, in which case it will be more efficient to start the computation at the minimal depth
    (depth = distance from the root) where at least one node is unknown. The minimal depth assumed
    to be completely correct is specified by min_depth_assumed_correct argument.

    For example, in a tree with 16 leaves, if the immediate parents of all the leaves were already
    computed because they were entered in pairs, using AddData(), the most efficient way to
    compute the root will be calling GetRoot(3). This is because depth 4 nodes are simply the leaves
    - which were explicitly fed into the tree, and we assume depth-3 nodes were computed implicitly,
    since the leaves were fed in pairs. Similarly, calling GetRoot(0) causes no hash operations to
    be performed, and simply returns the root stored from the last time it was computed.
  */
  HashT GetRoot(size_t min_depth_assumed_correct);

  void GenerateDecommitment(const std::set<uint64_t>& queries, ProverChannel* channel) const;

  static bool VerifyDecommitment(
      const std::map<uint64_t, HashT>& data_to_verify, uint64_t total_data_length,
      const HashT& merkle_root, VerifierChannel* channel);

  const uint64_t k_data_length;

 private:
  std::vector<HashT> nodes_;

  void SendDecommitmentNode(uint64_t node_index, ProverChannel* channel) const;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_MERKLE_MERKLE_H_
