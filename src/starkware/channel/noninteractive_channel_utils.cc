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

#include "starkware/channel/noninteractive_channel_utils.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "glog/logging.h"

#include "starkware/crypt_tools/template_instantiation.h"
#include "starkware/randomness/hash_chain.h"
#include "starkware/utils/serialization.h"

namespace starkware {

uint64_t NoninteractiveChannelUtils::GetRandomNumber(uint64_t upper_bound, PrngBase* prng) {
  std::array<std::byte, sizeof(uint64_t)> raw_bytes{};
  prng->GetRandomBytes(raw_bytes);
  uint64_t number = Deserialize<uint64_t>(raw_bytes);
  // The modulo operation will be random enough if the uppen bound is low compared to max uint64.
  ASSERT_RELEASE(
      upper_bound < 0x0001000000000000,
      "Random number with too high an upper bound");  // Ensures less than 1/2^16 non-uniformity in
                                                      // the modulo operation.
  return number % upper_bound;
}

}  // namespace starkware
