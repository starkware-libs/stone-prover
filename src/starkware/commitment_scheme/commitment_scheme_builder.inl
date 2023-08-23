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
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/merkle/merkle_commitment_scheme.h"
#include "starkware/commitment_scheme/packaging_commitment_scheme.h"
#include "starkware/commitment_scheme/parallel_table_prover.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/crypt_tools/invoke.h"
#include "starkware/crypt_tools/masked_hash.h"
#include "starkware/math/math.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

namespace commitment_scheme_builder {
namespace details {

inline size_t CalculateNVerifierFriendlyLayersInSegment(
    size_t n_segments, size_t n_layers_in_segment, size_t n_verifier_friendly_commitment_layers) {
  // No verifier friendly commitment layers at all.
  if (n_verifier_friendly_commitment_layers == 0) {
    return 0;
  }

  // The height of the top subtree with n_segments leaves.
  const size_t segment_tree_height = SafeLog2(n_segments);
  const size_t total_n_layers = n_layers_in_segment + segment_tree_height;

  if (n_verifier_friendly_commitment_layers >= total_n_layers) {
    // All layers are verifier friendly commitment layers.
    return n_layers_in_segment;
  }

  ASSERT_RELEASE(
      n_verifier_friendly_commitment_layers >= segment_tree_height,
      "The top " + std::to_string(segment_tree_height) +
          " layers should use the same hash. n_verifier_friendly_commitment_layers: " +
          std::to_string(n_verifier_friendly_commitment_layers));
  return n_verifier_friendly_commitment_layers - segment_tree_height;
}

/*
  Creates log(n_elements_in_segment) + 1 commitment scheme layers.
  Each layer is the inner layer of the next one.
  First, creates the innermost layer which holds the Merkle Tree.
  Then, creates in-memory commitment scheme layers which are implemented as interleaved layers of
  caching commitment schemes and packaging commitment schemes.
  Finally, creates n_out_of_memory_merkle_layers out of memory layers.
  Returns the outermost layer.
  Note: the commitment is done in a way that the data is split into segments and we commit to each
  segment separately. The smallest layer contains only one element in each segment. All these single
  elements form the leaves of a Merkle tree.
*/
inline std::unique_ptr<CommitmentSchemeProver> CreateAllCommitmentSchemeLayers(
    size_t n_out_of_memory_merkle_layers, size_t n_elements_in_segment, size_t n_segments,
    ProverChannel* channel, size_t n_verifier_friendly_commitment_layers,
    const CommitmentHashes& commitment_hashes) {
  // Creates the innermost layer which holds the Merkle Tree.
  // If it is bigger then 0 the innermost layer definitely uses the top hash (the entire segment
  // tree uses a single hash).
  const bool is_top_hash_layer = n_verifier_friendly_commitment_layers > 0;
  std::unique_ptr<CommitmentSchemeProver> next_inner_layer =
      commitment_hashes.Invoke(is_top_hash_layer, [n_segments, channel](auto hash_tag) {
        using hash_t = typename decltype(hash_tag)::type;
        auto commitment_scheme =
            std::make_unique<MerkleCommitmentSchemeProver<hash_t>>(n_segments, channel);
        return std::unique_ptr<CommitmentSchemeProver>(std::move(commitment_scheme));
      });

  const size_t n_layers_in_segment = SafeLog2(n_elements_in_segment);
  const size_t n_in_memory_layers =
      n_layers_in_segment - std::min(n_out_of_memory_merkle_layers, n_layers_in_segment);

  // n_verifier_friendly_commitment_layers is calculated from the root. We want to get the
  // relevant number in the segment height.
  const size_t n_verifier_friendly_layers_in_segment = CalculateNVerifierFriendlyLayersInSegment(
      n_segments, n_layers_in_segment, n_verifier_friendly_commitment_layers);
  ASSERT_RELEASE(
      n_verifier_friendly_layers_in_segment <= n_layers_in_segment,
      "n_verifier_friendly_layers_in_segment is too big");

  // Iterate over the layers from inner to outer. First creates in-memory layers, then out of
  // memory layers. (There are 2 elements in each segment in the innermost layer which is not the
  // Merkle tree layer (that was already created above)).
  size_t cur_n_elements_in_segment = 1;
  for (size_t layer = 0; layer < n_layers_in_segment; ++layer) {
    cur_n_elements_in_segment *= 2;
    ASSERT_RELEASE(
        cur_n_elements_in_segment <= n_elements_in_segment,
        "Too many elements in a segment: " + std::to_string(cur_n_elements_in_segment) +
            ". Should be at most: " + std::to_string(n_elements_in_segment));

    // Packaging commitment scheme layer.
    const bool is_top_hash_layer = (layer < n_verifier_friendly_layers_in_segment);
    next_inner_layer = commitment_hashes.Invoke(
        is_top_hash_layer,
        [cur_n_elements_in_segment, n_segments, channel, &next_inner_layer](auto hash_tag) {
          using hash_t = typename decltype(hash_tag)::type;

          auto commitment_scheme = std::make_unique<PackagingCommitmentSchemeProver<hash_t>>(
              hash_t::kDigestNumBytes, cur_n_elements_in_segment, n_segments, channel,
              std::move(next_inner_layer));
          return std::unique_ptr<CommitmentSchemeProver>(std::move(commitment_scheme));
        });
    // In memory caching commitment scheme layer.
    if (layer < n_in_memory_layers) {
      next_inner_layer = std::make_unique<CachingCommitmentSchemeProver>(
          next_inner_layer->ElementLengthInBytes(), cur_n_elements_in_segment, n_segments,
          std::move(next_inner_layer));
    }
  }
  return next_inner_layer;
}

/*
  Creates log(n_elements) + 1 commitment scheme layers for verification.
  Each layer is the inner layer of the next one.
  Returns the outermost layer.
*/
inline std::unique_ptr<CommitmentSchemeVerifier> CreateCommitmentSchemeVerifierLayers(
    size_t n_elements, VerifierChannel* channel, size_t n_verifier_friendly_commitment_layers,
    const CommitmentHashes& commitment_hashes) {
  const size_t n_layers = SafeLog2(n_elements);
  const size_t n_verifier_friendly_layers =
      std::min(n_layers, n_verifier_friendly_commitment_layers);

  // The most inner layer.
  size_t cur_n_elements_in_layer = 1;
  bool is_top_hash = n_verifier_friendly_layers > 0;
  std::unique_ptr<CommitmentSchemeVerifier> next_inner_layer =
      commitment_hashes.Invoke(is_top_hash, [cur_n_elements_in_layer, channel](auto hash_tag) {
        using hash_t = typename decltype(hash_tag)::type;
        auto commitment_scheme = std::make_unique<MerkleCommitmentSchemeVerifier<hash_t>>(
            cur_n_elements_in_layer, channel);
        return std::unique_ptr<CommitmentSchemeVerifier>(std::move(commitment_scheme));
      });

  // Create the rest of the layers, from inner to outer.
  for (size_t layer = 0; layer < n_layers; ++layer) {
    cur_n_elements_in_layer *= 2;
    ASSERT_RELEASE(
        cur_n_elements_in_layer <= n_elements,
        "Too many elements in layer number: " + std::to_string(layer) +
            ". # elements: " + std::to_string(cur_n_elements_in_layer) +
            ", but should be at most: " + std::to_string(n_elements));

    // Packaging commitment scheme layer.
    is_top_hash = layer < n_verifier_friendly_layers;
    next_inner_layer = commitment_hashes.Invoke(
        is_top_hash, [cur_n_elements_in_layer, channel, &next_inner_layer](auto hash_tag) {
          using hash_t = typename decltype(hash_tag)::type;
          auto commitment_scheme = std::make_unique<PackagingCommitmentSchemeVerifier<hash_t>>(
              hash_t::kDigestNumBytes, cur_n_elements_in_layer, channel,
              std::move(next_inner_layer));
          return std::unique_ptr<CommitmentSchemeVerifier>(std::move(commitment_scheme));
        });
  }
  return next_inner_layer;
}

}  // namespace details
}  // namespace commitment_scheme_builder

template <typename HashT>
PackagingCommitmentSchemeProver<HashT> MakeCommitmentSchemeProver(
    size_t size_of_element, size_t n_elements_in_segment, size_t n_segments, ProverChannel* channel,
    size_t n_verifier_friendly_commitment_layers, const CommitmentHashes& commitment_hashes,
    size_t n_out_of_memory_merkle_layers) {
  // Create a chain of in-memory layers and then out-of-memory layers. The smallest most inner
  // layer is a Merkle commitment scheme which holds a Merkle tree. For the outermost layer
  // is_merkle_layer == false, and it is not one of the out-of-memory Merkle layers.
  PackagingCommitmentSchemeProver<HashT> outer_layer(
      size_of_element, n_elements_in_segment, n_segments, channel,
      [n_out_of_memory_merkle_layers, n_segments, channel, n_verifier_friendly_commitment_layers,
       commitment_hashes](size_t n_elements_inner_layer) {
        return commitment_scheme_builder::details::CreateAllCommitmentSchemeLayers(
            n_out_of_memory_merkle_layers, SafeDiv(n_elements_inner_layer, n_segments), n_segments,
            channel, n_verifier_friendly_commitment_layers, commitment_hashes);
      });

  return outer_layer;
}

template <typename HashT>
PackagingCommitmentSchemeVerifier<HashT> MakeCommitmentSchemeVerifier(
    size_t size_of_element, uint64_t n_elements, VerifierChannel* channel,
    size_t n_verifier_friendly_commitment_layers, const CommitmentHashes& commitment_hashes) {
  // Create a chain of commitment scheme layers. The smallest most inner is a Merkle commitment
  // scheme which holds a Merkle tree. For the outermost layer is_merkle_layer == false.
  return PackagingCommitmentSchemeVerifier<HashT>(
      size_of_element, n_elements, channel,
      [channel, n_verifier_friendly_commitment_layers,
       commitment_hashes](size_t n_elements_inner_layer) {
        return commitment_scheme_builder::details::CreateCommitmentSchemeVerifierLayers(
            n_elements_inner_layer, channel, n_verifier_friendly_commitment_layers,
            commitment_hashes);
      },
      false);
}

}  // namespace starkware
