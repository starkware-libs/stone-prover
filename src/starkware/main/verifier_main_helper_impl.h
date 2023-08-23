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

#ifndef STARKWARE_MAIN_VERIFIER_MAIN_HELPER_IMPL_H_
#define STARKWARE_MAIN_VERIFIER_MAIN_HELPER_IMPL_H_

#include <string>
#include <vector>

#include "starkware/stark/stark.h"
#include "starkware/statement/statement.h"

namespace starkware {

/*
  Helper function for writing a main() function for STARK verifiers.
*/
bool VerifierMainHelperImpl(
    Statement* statement, const std::vector<std::byte>& proof, const JsonValue& parameters,
    const std::string& annotation_file_name, const std::string& extra_output_file_name);

}  // namespace starkware

#endif  // STARKWARE_MAIN_VERIFIER_MAIN_HELPER_IMPL_H_
