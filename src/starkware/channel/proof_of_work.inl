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
#include <atomic>

#include "third_party/cppitertools/range.hpp"

#include "starkware/utils/profiling.h"
#include "starkware/utils/serialization.h"

namespace starkware {

namespace proof_of_work {
namespace details {

template <typename HashT>
HashT InitHash(gsl::span<const std::byte> seed, size_t work_bits) {
  static constexpr std::array<std::byte, 8> kMagic =
      MakeByteArray<0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xed>();
  // init_bytes = kMagic || seed || work_bits.
  std::vector<std::byte> init_bytes(kMagic.begin(), kMagic.end());
  std::copy(seed.begin(), seed.end(), std::back_inserter(init_bytes));
  init_bytes.push_back(std::byte(work_bits));

  return HashT::HashBytesWithLength(init_bytes);
}

}  // namespace details
}  // namespace proof_of_work

template <typename HashT>
std::vector<std::byte> ProofOfWorkProver<HashT>::Prove(
    gsl::span<const std::byte> seed, size_t work_bits, uint64_t log_chunk_size) {
  return Prove(seed, work_bits, &TaskManager::GetInstance(), log_chunk_size);
}

template <typename HashT>
std::optional<std::uint64_t> SearchChunk(
    uint64_t nonce_start, uint64_t chunk_size, gsl::span<std::byte> thread_bytes,
    uint64_t work_limit) {
  gsl::span<std::byte> nonce_span = thread_bytes.last(sizeof(uint64_t));
  for (uint64_t nonce = nonce_start; nonce < nonce_start + chunk_size; ++nonce) {
    Serialize<uint64_t>(nonce, nonce_span, /*use_big_endian=*/true);
    const HashT hash = HashT::HashBytesWithLength(thread_bytes);
    const uint64_t digest_word = Deserialize<uint64_t>(
        gsl::make_span(hash.GetDigest()).first(sizeof(uint64_t)),
        /*use_big_endian=*/true);
    // Test we have enough zero bits.
    if (digest_word < work_limit) {
      return nonce;
    }
  }
  return std::nullopt;
}

template <typename HashT>
std::vector<std::byte> ProofOfWorkProver<HashT>::Prove(
    gsl::span<const std::byte> seed, size_t work_bits, TaskManager* task_manager,
    uint64_t log_chunk_size) {
  ASSERT_RELEASE(work_bits > 0, "At least one bits of work requires.");
  ASSERT_RELEASE(work_bits <= 64, "Too many bits of work requested");

  ProfilingBlock profiling_block("Proof of work");

  const HashT init_hash = proof_of_work::details::InitHash<HashT>(seed, work_bits);
  std::array<std::byte, HashT::kDigestNumBytes + sizeof(uint64_t)> bytes{};
  std::copy(init_hash.GetDigest().begin(), init_hash.GetDigest().end(), bytes.begin());

  const uint64_t work_limit = Pow2(64 - work_bits);
  const uint64_t chunk_size = Pow2(log_chunk_size);
  const size_t thread_count = (work_bits > log_chunk_size) ? task_manager->GetNumThreads() : 1;

  // We use nonce_bound to check for overflow (i.e. we finished searching 64 bits). In the beginning
  // every thread starts with nonce_start=thread_id*chunk_size, and before the second
  // iteration, nonce_start equals nonce_start+chunk_size, at which point an assert is made
  // that nonce_start >= nonce_bound.
  const uint64_t nonce_bound = thread_count * chunk_size;
  std::atomic_uint64_t next_chunk_to_search = nonce_bound;
  std::atomic_uint64_t lowest_nonce_found = std::numeric_limits<uint64_t>::max();
  task_manager->ParallelFor(
      thread_count, [&lowest_nonce_found, &next_chunk_to_search, &bytes, work_limit, chunk_size,
                     nonce_bound](const TaskInfo& task_info) {
        uint64_t thread_id = task_info.start_idx;
        std::array<std::byte, HashT::kDigestNumBytes + sizeof(uint64_t)> thread_bytes(bytes);
        uint64_t nonce_start = thread_id * chunk_size;
        do {
          std::optional<uint64_t> nonce =
              SearchChunk<HashT>(nonce_start, chunk_size, thread_bytes, work_limit);
          if (nonce.has_value()) {
            // If a valid nonce was found, check if it is smaller than lowest_nonce_found, and if it
            // is, loop until one of the following happens:
            // 1) lowest_nonce_found is changed to nonce by the current thread.
            // 2) lowest_nonce_found is set to a value smaller than nonce by a different thread.
            std::uint64_t curr_nonce = lowest_nonce_found.load();
            while (*nonce < curr_nonce &&
                   !std::atomic_compare_exchange_strong(&lowest_nonce_found, &curr_nonce, *nonce)) {
            }
          }
          // Get the next available nonce, and atomically add to it chunk_size.
          nonce_start = next_chunk_to_search.fetch_add(chunk_size);
        } while (nonce_start < lowest_nonce_found.load() && nonce_start >= nonce_bound);
      });
  uint64_t nonce_as_uint64 = lowest_nonce_found.load();
  ASSERT_RELEASE(nonce_as_uint64 != std::numeric_limits<uint64_t>::max(), "No nonce was found.");
  std::vector<std::byte> nonce(sizeof(uint64_t));
  Serialize<uint64_t>(nonce_as_uint64, nonce, /*use_big_endian=*/true);
  return nonce;
}

template <typename HashT>
bool ProofOfWorkVerifier<HashT>::Verify(
    gsl::span<const std::byte> seed, size_t work_bits, gsl::span<const std::byte> nonce_bytes) {
  ASSERT_RELEASE(work_bits > 0, "At least one bits of work requires.");
  ASSERT_RELEASE(work_bits <= 64, "Too many bits of work requested");

  const HashT init_hash = proof_of_work::details::InitHash<HashT>(seed, work_bits);
  std::array<std::byte, HashT::kDigestNumBytes + sizeof(uint64_t)> bytes{};
  std::copy(init_hash.GetDigest().begin(), init_hash.GetDigest().end(), bytes.begin());
  std::copy(nonce_bytes.begin(), nonce_bytes.end(), bytes.begin() + HashT::kDigestNumBytes);
  const uint64_t work_limit = Pow2(64 - work_bits);

  const HashT hash = HashT::HashBytesWithLength(bytes);
  const uint64_t digest_word = Deserialize<uint64_t>(
      gsl::make_span(hash.GetDigest()).first(sizeof(uint64_t)), /*use_big_endian=*/true);

  return digest_word < work_limit;
}

}  // namespace starkware
