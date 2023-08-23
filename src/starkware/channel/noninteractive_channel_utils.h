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

#ifndef STARKWARE_CHANNEL_NONINTERACTIVE_CHANNEL_UTILS_H_
#define STARKWARE_CHANNEL_NONINTERACTIVE_CHANNEL_UTILS_H_

#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/polymorphic/field.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/randomness/hash_chain.h"

namespace starkware {

class NoninteractiveChannelUtils {
 public:
  NoninteractiveChannelUtils() = default;

  static uint64_t GetRandomNumber(uint64_t upper_bound, PrngBase* prng);
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_NONINTERACTIVE_CHANNEL_UTILS_H_
