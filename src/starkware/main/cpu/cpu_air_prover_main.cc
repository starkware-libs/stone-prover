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
  Creates a proof for the claim:
    I executed a program with a given initial memory (which includes the program and its public
    input). It ran successfully and ended at the given pc.
*/

#include "gflags/gflags.h"

#include "starkware/main/prover_main_helper.h"
#include "starkware/main/prover_version.h"
#include "starkware/statement/cpu/cpu_air_statement.h"
#include "starkware/utils/profiling.h"
#include "starkware/utils/stats.h"

int main(int argc, char** argv) {
  using namespace starkware;       // NOLINT
  using namespace starkware::cpu;  // NOLINT
  gflags::SetVersionString(GetProverVersionString());
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);  // NOLINT

  CpuAirStatement statement(GetParametersInput()["statement"], GetPublicInput(), GetPrivateInput());
  ProfilingBlock profiling_block("Prover", 0);
  ProverMainHelper(&statement, GetProverVersion());
  WriteStats();

  return 0;
}
