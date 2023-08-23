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

#ifndef STARKWARE_UTILS_INPUT_UTILS_H_
#define STARKWARE_UTILS_INPUT_UTILS_H_

#include <string>
#include <vector>

#include "starkware/utils/json.h"

namespace starkware {

JsonValue GetConfigJson(
    bool store_full_lde, bool use_fft_for_eval = false,
    Json::UInt table_prover_n_tasks_per_segment = 32,
    Json::UInt constraint_polynomial_task_size = 256, Json::UInt n_out_of_memory_merkle_layers = 1,
    Json::UInt max_non_chunked_layer_size = 32768, Json::UInt n_chunks_between_layers = 32,
    Json::UInt log_n_max_in_memory_fri_layer_elements = 63);

JsonValue GetParametersJson(
    uint64_t trace_length, uint64_t log_n_cosets = 4, uint64_t security_bits = 80,
    uint64_t proof_of_work_bits = 20, bool should_add_zero_layer = true,
    const std::string& field_name = "PrimeField0", bool use_extension_field = false);

}  // namespace starkware

#endif  // STARKWARE_UTILS_INPUT_UTILS_H_
