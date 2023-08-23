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

#include "starkware/main/prover_main_helper_impl.h"

#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gflags/gflags.h"

#include "starkware/algebra/fields/field_operations_helper.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/crypt_tools/invoke.h"
#include "starkware/crypt_tools/masked_hash.h"
#include "starkware/stark/stark.h"
#include "starkware/stark/utils.h"
#include "starkware/utils/flag_validators.h"
#include "starkware/utils/json_builder.h"
#include "starkware/utils/maybe_owned_ptr.h"
#include "starkware/utils/profiling.h"

namespace starkware {

static JsonValue StringToJsonArray(const std::string& string_with_newlines) {
  JsonBuilder output;
  output["array"] = JsonValue::EmptyArray();
  auto array_json = output["array"];
  std::istringstream stream(string_with_newlines);
  std::string line;
  while (getline(stream, line)) {
    array_json.Append(line);
  }
  auto json = output.Build();
  return json["array"];
}
/*
  Save unified output of the prover (including the inputs).
*/
static void SaveUnitedProverOutput(
    const std::string& file_name, const JsonValue& private_input, const JsonValue& public_input,
    const JsonValue& parameters, const JsonValue& prover_config, const std::string& proof,
    const std::string& annotations, const ProverVersion& prover_version) {
  JsonBuilder output;

  output["version"]["statement_name"] = prover_version.statement_name;
  output["version"]["proof_hash"] = prover_version.proof_hash;
  output["version"]["commit_hash"] = prover_version.commit_hash;

  output["private_input"] = private_input;
  output["public_input"] = public_input;
  output["proof_parameters"] = parameters;
  output["prover_config"] = prover_config;
  output["proof_hex"] = proof;
  if (!annotations.empty()) {
    output["annotations"] = StringToJsonArray(annotations);
  }
  output.Build().Write(file_name);
}

std::vector<std::byte> ProverMainHelperImpl(
    Statement* statement, const JsonValue& parameters, const JsonValue& stark_config_json,
    const JsonValue& public_input, const std::string& out_file_name, bool generate_annotations,
    const ProverVersion& prover_version) {
  const Air& air = statement->GetAir();

  StarkProverConfig stark_config(StarkProverConfig::FromJson(stark_config_json));
  const bool use_extension_field = parameters["use_extension_field"].AsBool();
  Field field = parameters["field"].AsField();
  if (use_extension_field) {
    ASSERT_RELEASE(
        IsExtensionField(field),
        "use_extension_field is true but the field is not an extension field.");
  }

  StarkParameters stark_params =
      StarkParameters::FromJson(parameters["stark"], field, UseOwned(&air), use_extension_field);

  const std::string channel_hash =
      parameters["channel_hash"].HasValue() ? parameters["channel_hash"].AsString() : "keccak256";

  std::unique_ptr<PrngBase> prng = InvokeByHashFunc(channel_hash, [&](auto hash_tag) {
    using HashT = typename decltype(hash_tag)::type;
    PrngImpl<HashT> prng(statement->GetInitialHashChainSeed());
    return prng.Clone();
  });

  NoninteractiveProverChannel channel(std::move(prng));
  if (!generate_annotations) {
    channel.DisableAnnotations();
  }

  const std::string commitment_hash = parameters["commitment_hash"].HasValue()
                                          ? parameters["commitment_hash"].AsString()
                                          : "keccak256_masked160_msb";

  const std::string verifier_friendly_commitment_hash =
      parameters["verifier_friendly_commitment_hash"].HasValue()
          ? parameters["verifier_friendly_commitment_hash"].AsString()
          : commitment_hash;

  // Note that n_verifier_friendly_commitment_layers params needs be either 0 or at least
  // log(table_prover_n_tasks_per_segment) * n_cosets. See
  // CalculateNVerifierFriendlyLayersInSegment in
  // src/starkware/commitment_scheme/commitment_scheme_builder.inl.
  const size_t n_verifier_friendly_commitment_layers =
      parameters["n_verifier_friendly_commitment_layers"].HasValue()
          ? parameters["n_verifier_friendly_commitment_layers"].AsUint64()
          : 0;

  TableProverFactory table_prover_factory = InvokeByHashFunc(commitment_hash, [&](auto hash_tag) {
    using HashT = typename decltype(hash_tag)::type;
    return GetTableProverFactory<HashT>(
        &channel, stark_params.field.ElementSizeInBytes(),
        stark_config.table_prover_n_tasks_per_segment, stark_config.n_out_of_memory_merkle_layers,
        n_verifier_friendly_commitment_layers,
        CommitmentHashes(
            /*top_hash=*/verifier_friendly_commitment_hash, /*bottom_hash=*/commitment_hash));
  });

  AnnotationScope scope(&channel, statement->GetName());
  StarkProver prover(
      UseOwned(&channel), UseOwned(&table_prover_factory), UseOwned(&stark_params),
      UseOwned(&stark_config));
  // Note that in case there is an interaction, ProveStark creates a new Air with interaction
  // elements, which is destroyed when the function ends.
  prover.ProveStark(statement->GetTraceContext());

  std::vector<std::byte> proof_bytes = channel.GetProof();

  // Print statistics.
  LOG(INFO) << channel.GetStatistics().ToString();

  if (!out_file_name.empty()) {
    std::ostringstream annotations;
    if (generate_annotations) {
      annotations << channel;
    }
    SaveUnitedProverOutput(
        out_file_name, statement->GetPrivateInput(), public_input, parameters, stark_config_json,
        BytesToHexString(proof_bytes, false), annotations.str(), prover_version);
  }
  return proof_bytes;
}

}  // namespace starkware
