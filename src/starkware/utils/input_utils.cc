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

#include "starkware/algebra/fields/field_operations_helper.h"
#include "starkware/algebra/utils/name_to_field.h"
#include "starkware/utils/input_utils.h"
#include "starkware/utils/json_builder.h"

namespace starkware {

JsonValue GetConfigJson(
    bool store_full_lde, bool use_fft_for_eval, Json::UInt table_prover_n_tasks_per_segment,
    Json::UInt constraint_polynomial_task_size, Json::UInt n_out_of_memory_merkle_layers,
    Json::UInt max_non_chunked_layer_size, Json::UInt n_chunks_between_layers,
    Json::UInt log_n_max_in_memory_fri_layer_elements) {
  JsonBuilder output;

  output["cached_lde_config"]["store_full_lde"] = store_full_lde;
  output["cached_lde_config"]["use_fft_for_eval"] = use_fft_for_eval;

  output["table_prover_n_tasks_per_segment"] = table_prover_n_tasks_per_segment;
  output["constraint_polynomial_task_size"] = constraint_polynomial_task_size;
  output["n_out_of_memory_merkle_layers"] = n_out_of_memory_merkle_layers;

  output["fri_prover"]["max_non_chunked_layer_size"] = max_non_chunked_layer_size;
  output["fri_prover"]["n_chunks_between_layers"] = n_chunks_between_layers;
  output["fri_prover"]["log_n_max_in_memory_fri_layer_elements"] =
      log_n_max_in_memory_fri_layer_elements;

  return output.Build();
}

JsonValue GetParametersJson(
    uint64_t trace_length, uint64_t log_n_cosets, uint64_t security_bits,
    uint64_t proof_of_work_bits, bool should_add_zero_layer, const std::string& field_name,
    bool use_extension_field) {
  if (use_extension_field) {
    auto field = NameToField(field_name);
    ASSERT_RELEASE(
        field.has_value() && IsExtensionField(field.value()),
        "use_extension_field is true but the field is not an extension field.");
  }
  JsonBuilder params;

  uint64_t log_degree_bound = SafeLog2(trace_length);
  uint64_t last_layer_log_deg = std::min<uint64_t>(6, log_degree_bound);
  uint64_t n_queries = (security_bits - proof_of_work_bits + log_n_cosets - 1) / log_n_cosets;

  if (should_add_zero_layer) {
    params["stark"]["fri"]["fri_step_list"].Append(0U);
  }

  uint64_t spare_degree = log_degree_bound - last_layer_log_deg;
  while (spare_degree > 1) {
    uint64_t curr_step = std::min<uint64_t>(3, spare_degree);
    params["stark"]["fri"]["fri_step_list"].Append(curr_step);
    spare_degree -= curr_step;
  }

  // Our Solidity verifier doesn't support FRI steps of 1.
  // We avoid this by adding a step of 2 and dividing the last layer degree by 2.
  if (spare_degree == 1) {
    last_layer_log_deg = 5;
    params["stark"]["fri"]["fri_step_list"].Append(2);
  }

  params["field"] = field_name;
  params["use_extension_field"] = use_extension_field;
  params["stark"]["log_n_cosets"] = log_n_cosets;
  params["stark"]["fri"]["last_layer_degree_bound"] = Pow2(last_layer_log_deg);
  params["stark"]["fri"]["n_queries"] = n_queries;
  params["stark"]["fri"]["proof_of_work_bits"] = proof_of_work_bits;

  return params.Build();
}

}  // namespace starkware
