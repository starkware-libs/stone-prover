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

#include <fstream>

#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/crypt_tools/masked_hash.h"

namespace starkware {

std::vector<std::byte> ReadProof(const std::string& file_name) {
  std::ifstream proof_file(file_name, std::ios::binary);
  ASSERT_RELEASE(proof_file.is_open(), "Failed to open proof file.");
  std::vector<std::byte> res;

  for (auto it = std::istreambuf_iterator<char>{proof_file}; it != std::istreambuf_iterator<char>{};
       ++it) {
    res.push_back(std::byte(*it));
  }
  return res;
}

TableProverFactory GetDefaultTableProverFactory(
    ProverChannel* channel, size_t field_element_size_in_bytes, size_t n_tasks_per_segment,
    size_t n_out_of_memory_merkle_layers, size_t n_verifier_friendly_commitment_layers) {
  return GetTableProverFactory<MaskedHash<Keccak256, 20, true>>(
      channel, field_element_size_in_bytes, n_tasks_per_segment, n_out_of_memory_merkle_layers,
      n_verifier_friendly_commitment_layers, CommitmentHashes(Keccak256::HashName()));
}

}  // namespace starkware
