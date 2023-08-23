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

/*
  Verifies a CPU AIR proof. See cpu_air_prover_main.cc for details.
*/

#include <optional>

#include "gflags/gflags.h"

#include "starkware/main/verifier_main_helper.h"
#include "starkware/statement/cpu/cpu_air_statement.h"

int main(int argc, char** argv) {
  using namespace starkware;       // NOLINT
  using namespace starkware::cpu;  // NOLINT
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);  // NOLINT

  auto factory = [](const JsonValue& public_input, const JsonValue& parameters) {
    return std::unique_ptr<Statement>(
        new CpuAirStatement(parameters["statement"], public_input, std::nullopt));
  };
  bool result = starkware::VerifierMainHelper(factory);

  if (result) {
    LOG(INFO) << "Proof verified successfully.";
  } else {
    LOG(ERROR) << "Invalid proof.";
  }

  return result ? 0 : 1;
}
