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

#ifndef STARKWARE_CHANNEL_CHANNEL_STATISTICS_H_
#define STARKWARE_CHANNEL_CHANNEL_STATISTICS_H_

#include <string>

namespace starkware {

class ChannelStatistics {
 public:
  ChannelStatistics() = default;

  std::string ToString() const {
    std::string stats = "Byte count: " + std::to_string(byte_count) + "\n" +
                        "Hash count: " + std::to_string(hash_count) + "\n" +
                        "Commitment count: " + std::to_string(commitment_count) + "\n" +
                        "Field element count: " + std::to_string(field_element_count) + "\n" +
                        "Data count: " + std::to_string(data_count) + "\n";
    return stats;
  }

  size_t byte_count = 0;
  size_t hash_count = 0;
  size_t commitment_count = 0;
  size_t field_element_count = 0;
  size_t data_count = 0;
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_CHANNEL_STATISTICS_H_
