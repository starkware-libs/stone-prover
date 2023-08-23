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

#ifndef STARKWARE_MAIN_VERIFIER_MAIN_HELPER_H_
#define STARKWARE_MAIN_VERIFIER_MAIN_HELPER_H_

#include <functional>
#include <memory>

#include "starkware/statement/statement.h"

namespace starkware {

using StatementFactory = std::function<std::unique_ptr<Statement>(
    const JsonValue& public_input, const JsonValue& parameters)>;

/*
  Helper function for writing a main() function for STARK verifiers.
*/
bool VerifierMainHelper(const StatementFactory& statement_factory);

}  // namespace starkware

#endif  // STARKWARE_MAIN_VERIFIER_MAIN_HELPER_H_
