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

#include "starkware/main/verifier_main_helper.h"

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/main/verifier_main_helper_impl.h"
#include "starkware/proof_system/proof_system.h"
#include "starkware/randomness/prng.h"
#include "starkware/utils/flag_validators.h"

DEFINE_string(in_file, "", "Path to the unified input file.");
DEFINE_validator(in_file, &starkware::ValidateInputFile);

DEFINE_string(
    extra_output_file, "",
    "Optional. Path to the output file that will contain extra data necessary for "
    "generating split proofs.");
DEFINE_validator(extra_output_file, &starkware::ValidateOptionalOutputFile);

DEFINE_string(
    annotation_file, "",
    "Optional. Path to the output file that will contain the annotated proof.");
DEFINE_validator(annotation_file, &starkware::ValidateOptionalOutputFile);

namespace starkware {

namespace {

JsonValue ReadInputJson(const std::string& in_file_name) {
  ASSERT_DEBUG(!in_file_name.empty(), "Input file must be given");
  return JsonValue::FromFile(in_file_name);
}

struct VerifierParameters {
  JsonValue public_input;
  JsonValue parameters;
  std::vector<std::byte> proof;
};

VerifierParameters GetVerifierParameters() {
  JsonValue input_json = ReadInputJson(FLAGS_in_file);
  std::string proof_hex = input_json["proof_hex"].AsString();
  std::vector<std::byte> proof((proof_hex.size() - 1) / 2);
  starkware::HexStringToBytes(proof_hex, proof);
  return {input_json["public_input"], input_json["proof_parameters"], proof};
}

}  // namespace

bool VerifierMainHelper(const StatementFactory& statement_factory) {
  VerifierParameters verifier_params = GetVerifierParameters();

  std::unique_ptr<starkware::Statement> statement =
      statement_factory(verifier_params.public_input, verifier_params.parameters);
  return starkware::VerifierMainHelperImpl(
      statement.get(), verifier_params.proof, verifier_params.parameters, FLAGS_annotation_file,
      FLAGS_extra_output_file);
}

}  // namespace starkware
