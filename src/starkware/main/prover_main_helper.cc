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

#include "starkware/main/prover_main_helper.h"

#include <sys/resource.h>
#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include "gflags/gflags.h"

#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/main/prover_main_helper_impl.h"
#include "starkware/stark/stark.h"
#include "starkware/stark/utils.h"
#include "starkware/utils/flag_validators.h"

DEFINE_string(private_input_file, "", "Path to the json file containing the private input.");
DEFINE_validator(private_input_file, &starkware::ValidateInputFile);

DEFINE_bool(fix_public_input, false, "Re-compute public input");

DEFINE_string(
    out_file, "", "Path to the unified output file that will contain the output and input data.");
DEFINE_validator(out_file, &starkware::ValidateOutputFile);

DEFINE_string(
    prover_config_file, "",
    "Path to the json file containing parameters controlling the prover optimization parameters.");
DEFINE_validator(prover_config_file, &starkware::ValidateInputFile);

DEFINE_bool(generate_annotations, false, "Optional. Generate proof annotations.");

DEFINE_string(parameter_file, "", "Path to the json file containing the proof parameters.");
DEFINE_validator(parameter_file, &starkware::ValidateInputFile);

DEFINE_string(public_input_file, "", "Path to the json file containing the public input.");
DEFINE_validator(public_input_file, &starkware::ValidateInputFile);

namespace starkware {

namespace {

void DisableCoreDump() {
  struct rlimit rlim {};

  rlim.rlim_cur = rlim.rlim_max = 0;
  setrlimit(RLIMIT_CORE, &rlim);
}

}  // namespace

JsonValue GetPrivateInput() { return JsonValue::FromFile(FLAGS_private_input_file); }
JsonValue GetStarkProverConfig() { return JsonValue::FromFile(FLAGS_prover_config_file); }
JsonValue GetPublicInput() { return JsonValue::FromFile(FLAGS_public_input_file); }
JsonValue GetParametersInput() { return JsonValue::FromFile(FLAGS_parameter_file); }

void ProverMainHelper(Statement* statement, const ProverVersion& prover_version) {
  // Disable core dumps to save storage space.
  DisableCoreDump();

  JsonValue public_input = FLAGS_fix_public_input ? statement->FixPublicInput() : GetPublicInput();
  ProverMainHelperImpl(
      statement, GetParametersInput(), GetStarkProverConfig(), public_input, FLAGS_out_file,
      FLAGS_generate_annotations, prover_version);
}

}  // namespace starkware
