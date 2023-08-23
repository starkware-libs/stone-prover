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

#ifndef STARKWARE_MAIN_PROVER_VERSION_H_
#define STARKWARE_MAIN_PROVER_VERSION_H_

#include <string>

#ifndef STR

#define PROVER_NAME INVALID_NAME
#define PROOF_HASH INVALID_PROOF_HASH
#define COMMIT_HASH INVALID_COMMIT

#define STR_(x) #x
#define STR(x) STR_(x)
#endif

namespace starkware {

struct ProverVersion {
  const std::string statement_name;
  const std::string proof_hash;
  const std::string commit_hash;
};

inline auto GetProverVersion() {
  return ProverVersion{STR(PROVER_NAME), STR(PROOF_HASH), STR(COMMIT_HASH)};
}

inline auto GetProverVersion(const std::string& name, const std::string& proof_hash) {
  return ProverVersion{name, proof_hash, STR(COMMIT_HASH)};
}

inline auto GetProverVersionString() {
  ProverVersion prover_version = GetProverVersion();
  return prover_version.statement_name + "-" + prover_version.proof_hash + "-" +
         prover_version.commit_hash;
}

}  // namespace starkware

#endif  // STARKWARE_MAIN_PROVER_VERSION_H_
