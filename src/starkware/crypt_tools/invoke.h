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

#ifndef STARKWARE_CRYPT_TOOLS_INVOKE_H_
#define STARKWARE_CRYPT_TOOLS_INVOKE_H_

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include "starkware/algebra/utils/invoke_template_version.h"
#include "starkware/crypt_tools/blake2s.h"
#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/crypt_tools/masked_hash.h"
#include "starkware/crypt_tools/pedersen.h"

namespace starkware {

using HashTypes = InvokedTypes<
    Blake2s256, Keccak256, Pedersen, MaskedHash<Keccak256, 20, true>,
    MaskedHash<Blake2s256, 20, true>, MaskedHash<Blake2s256, 20, false>,
    MaskedHash<Keccak256, 20, false>>;

template <typename Func>
auto InvokeByHashFunc(const std::string& hash_name, const Func& func) {
  return InvokeGenericTemplateVersion<HashTypes>(func, [&](auto hash_tag) {
    using HashT = typename decltype(hash_tag)::type;
    return hash_name == HashT::HashName();
  });
}

/*
  Auxiliary class to be used in commitment scheme. Enables to choose between two different hashes,
  one for the top Merkle tree layers and one for the bottom Merkle tree layers.
  Used to invoke arbitrary functions with the correct hash.
*/
class CommitmentHashes {
 public:
  CommitmentHashes(std::string top_hash, std::string bottom_hash)
      : top_hash_(std::move(top_hash)), bottom_hash_(std::move(bottom_hash)) {}

  explicit CommitmentHashes(const std::string& hash) : CommitmentHashes(hash, hash) {}

  template <typename Func>
  auto Invoke(bool is_top_hash_layer, const Func& func) const {
    return InvokeByHashFunc(GetHashName(is_top_hash_layer), func);
  }

 private:
  const std::string& GetHashName(bool is_top_hash_layer) const {
    return is_top_hash_layer ? top_hash_ : bottom_hash_;
  }

  const std::string top_hash_;
  const std::string bottom_hash_;
};

}  // namespace starkware

#endif  // STARKWARE_CRYPT_TOOLS_INVOKE_H_
