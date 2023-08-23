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

#ifndef STARKWARE_MAIN_PROVER_MAIN_HELPER_H_
#define STARKWARE_MAIN_PROVER_MAIN_HELPER_H_

#include "starkware/main/prover_version.h"
#include "starkware/stark/stark.h"
#include "starkware/statement/statement.h"

namespace starkware {

/*
  Helper function for writing a main() function for STARK provers.
*/
void ProverMainHelper(Statement* statement, const ProverVersion& prover_version);

/*
  Reads the json file specified by the --private_input flag.
*/
JsonValue GetPrivateInput();

/*
  Reads the json file specified by the --public_input flag.
*/
JsonValue GetPublicInput();

/*
  Read the json file specified by the --parameter_file flag.
*/
JsonValue GetParametersInput();

}  // namespace starkware

#endif  // STARKWARE_MAIN_PROVER_MAIN_HELPER_H_
