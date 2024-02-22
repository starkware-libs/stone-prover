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

#include "starkware/stark/utils.h"

#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/parallel_table_prover.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/math/math.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

template <typename HashT>
TableProverFactory GetTableProverFactory(
    ProverChannel* channel, size_t field_element_size_in_bytes, size_t n_tasks_per_segment,
    size_t n_out_of_memory_merkle_layers, size_t n_verifier_friendly_commitment_layers,
    const CommitmentHashes& commitment_hashes) {
  return [channel, field_element_size_in_bytes, n_tasks_per_segment, n_out_of_memory_merkle_layers,
          n_verifier_friendly_commitment_layers, commitment_hashes](
             size_t n_segments, uint64_t n_rows_per_segment,
             size_t n_columns) -> std::unique_ptr<TableProver> {
    bool use_parallel_prover = false;
    if (n_rows_per_segment >= n_tasks_per_segment &&
        // CommitmentSchemeT requires each segment to have at least 2 * HashT::kDigestNumBytes
        // bytes. Avoid spliting the segments if it gets us bellow that threshold.
        field_element_size_in_bytes * n_rows_per_segment * n_columns >
            n_tasks_per_segment * 2 * HashT::kDigestNumBytes &&
        n_tasks_per_segment > 1) {
      // Break each segment into smaller sub segments (fewer rows per segment) for
      // parallel processing.
      // Note that we support only  powers of 2 for n_rows_per_segment and the n_segments.
      ASSERT_RELEASE(
          IsPowerOfTwo(n_rows_per_segment), "Expecting n_rows_per_segment to be a power of two.");
      ASSERT_RELEASE(IsPowerOfTwo(n_segments), "Expecting n_segments to be a power of two.");
      size_t log_n_tasks_per_segment = SafeLog2(n_tasks_per_segment);
      n_rows_per_segment /= Pow2(log_n_tasks_per_segment);
      n_segments *= Pow2(log_n_tasks_per_segment);
      use_parallel_prover = true;
    }

    auto packaging_commitment_scheme = MakeCommitmentSchemeProver<HashT>(
        field_element_size_in_bytes * n_columns, n_rows_per_segment, n_segments, channel,
        n_verifier_friendly_commitment_layers, commitment_hashes, n_out_of_memory_merkle_layers);

    auto table_prover = std::make_unique<TableProverImpl>(
        n_columns, UseMovedValue(std::move(packaging_commitment_scheme)), channel);

    if (!use_parallel_prover) {
      return table_prover;
    }

    return std::make_unique<ParallelTableProver>(
        TakeOwnershipFrom(std::move(table_prover)), n_tasks_per_segment, n_rows_per_segment);
  };
}

}  // namespace starkware
