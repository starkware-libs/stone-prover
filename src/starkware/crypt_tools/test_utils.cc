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

#include "starkware/crypt_tools/test_utils.h"

namespace starkware {

std::vector<std::byte> GenerateTestVector(size_t length) {
  std::vector<std::byte> test_vector;
  test_vector.reserve(length);

  char val = 0x11;
  for (size_t i = 0; i < length; i++) {
    test_vector.push_back(std::byte(val));
    val += 0x11;
    if (val == static_cast<char>(0x99)) {
      val = 0x11;
    }
  }

  return test_vector;
}

}  // namespace starkware
