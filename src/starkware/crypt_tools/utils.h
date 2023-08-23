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

#ifndef STARKWARE_CRYPT_TOOLS_UTILS_H_
#define STARKWARE_CRYPT_TOOLS_UTILS_H_

#include <algorithm>
#include <cstddef>
#include <string>

#include "starkware/error_handling/error_handling.h"
#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

template <typename HashT>
inline std::array<std::byte, HashT::kDigestNumBytes> InitDigestFromSpan(
    const gsl::span<const std::byte>& data) {
  ASSERT_VERIFIER(
      data.size() == HashT::kDigestNumBytes,
      "Hash expects input of the length of a single digest: " +
          std::to_string(HashT::kDigestNumBytes) + " but got: " + std::to_string(data.size()));
  std::array<std::byte, HashT::kDigestNumBytes> digest;  // NOLINT
  std::copy(data.begin(), data.end(), digest.begin());
  return digest;
}

}  // namespace starkware

#endif  // STARKWARE_CRYPT_TOOLS_UTILS_H_
