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

#ifndef STARKWARE_MAIN_PROVER_MAIN_HELPER_IMPL_H_
#define STARKWARE_MAIN_PROVER_MAIN_HELPER_IMPL_H_

#include <cstddef>
#include <string>
#include <vector>

#include "starkware/main/prover_version.h"
#include "starkware/stark/stark.h"
#include "starkware/statement/statement.h"

namespace starkware {

/*
  Helper function that reads the configurations from objects (instead of files) and returns the
  proof generated. If a path proof_file_name is given, the proof is written to it.
*/
std::vector<std::byte> ProverMainHelperImpl(
    Statement* statement, const JsonValue& parameters, const JsonValue& stark_config_json,
    const JsonValue& public_input, const std::string& out_file_name = "",
    bool generate_annotations = false, const ProverVersion& prover_version = {});

}  // namespace starkware

#endif  // STARKWARE_MAIN_PROVER_MAIN_HELPER_IMPL_H_
