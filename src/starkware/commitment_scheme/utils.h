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

#ifndef STARKWARE_COMMITMENT_SCHEME_UTILS_H_
#define STARKWARE_COMMITMENT_SCHEME_UTILS_H_

#include <vector>

#include "starkware/math/math.h"

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

/*
  Given span of bytes, bytes_data, converts and returns them in the format of vector of n_elements
  hashes of HashT.
*/
template <typename HashT>
inline std::vector<HashT> BytesAsHash(
    const gsl::span<const std::byte>& bytes_data, const size_t size_of_element) {
  std::vector<HashT> bytes_as_hash;
  const size_t n_elements = SafeDiv(bytes_data.size(), size_of_element);
  bytes_as_hash.reserve(n_elements);
  for (size_t hash_idx = 0; hash_idx < n_elements; hash_idx++) {
    const size_t offset = hash_idx * size_of_element;
    bytes_as_hash.push_back(HashT::InitDigestTo(bytes_data.subspan(offset, size_of_element)));
  }
  return bytes_as_hash;
}

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_UTILS_H_
