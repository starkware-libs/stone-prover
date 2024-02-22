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

#ifndef STARKWARE_STARK_UTILS_H_
#define STARKWARE_STARK_UTILS_H_

#include <cstddef>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/prover_channel.h"
#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/crypt_tools/invoke.h"

namespace starkware {

std::vector<std::byte> ReadProof(const std::string& file_name);

TableProverFactory GetDefaultTableProverFactory(
    ProverChannel* channel, size_t field_element_size_in_bytes, size_t n_tasks_per_segment,
    size_t n_out_of_memory_merkle_layers, size_t n_verifier_friendly_commitment_layers);

template <typename HashT>
TableProverFactory GetTableProverFactory(
    ProverChannel* channel, size_t field_element_size_in_bytes, size_t n_tasks_per_segment,
    size_t n_out_of_memory_merkle_layers, size_t n_verifier_friendly_commitment_layers,
    const CommitmentHashes& commitment_hashes);

}  // namespace starkware

#include "starkware/stark/utils.inl"

#endif  // STARKWARE_STARK_UTILS_H_
