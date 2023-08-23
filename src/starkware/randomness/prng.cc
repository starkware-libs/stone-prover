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

#include "starkware/randomness/prng.h"

#include <chrono>

#include "gflags/gflags.h"
#include "glog/logging.h"

#include "starkware/crypt_tools/blake2s.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/serialization.h"
#include "starkware/utils/to_from_string.h"

DEFINE_string(override_random_seed, "", "override seed for prng");

namespace starkware {

namespace {

std::array<std::byte, sizeof(uint64_t)> SeedFromSystemTime() {
  uint64_t seed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();

  std::array<std::byte, sizeof(uint64_t)> seed_bytes{};
  Serialize<uint64_t>(seed, seed_bytes);

  return seed_bytes;
}

std::array<std::byte, sizeof(uint64_t)> SeedPrintoutToBytes(const std::string& printout) {
  std::array<std::byte, sizeof(uint64_t)> seed_bytes{};
  HexStringToBytes(printout, seed_bytes);
  return seed_bytes;
}

std::string BytesToSeedPrintout(const std::array<std::byte, sizeof(uint64_t)>& seed_bytes) {
  return BytesToHexString(seed_bytes);
}

}  // namespace

template <typename HashT>
PrngImpl<HashT>::PrngImpl() {
  const std::array<std::byte, sizeof(uint64_t)> seed_bytes =
      FLAGS_override_random_seed.empty() ? SeedFromSystemTime()
                                         : SeedPrintoutToBytes(FLAGS_override_random_seed);
  const std::string seed_string = BytesToSeedPrintout(seed_bytes);
  LOG(ERROR) << "Seeding PRNG with " << seed_string;
  ASSERT_DEBUG(
      SeedPrintoutToBytes(seed_string) == seed_bytes, "Randomness not reproducible from printout.");
  Reseed(seed_bytes);
}

template <typename HashT>
PrngImpl<HashT> PrngImpl<HashT>::FromPrintout(const std::string& printout) {
  return PrngImpl<HashT>(SeedPrintoutToBytes(printout));
}

template <typename HashT>
std::vector<bool> PrngImpl<HashT>::UniformBoolVector(size_t n_elements) {
  auto bits = UniformIntVector<uint8_t>(0, 1, n_elements);
  return {bits.begin(), bits.end()};
}

template class PrngImpl<Keccak256>;
template class PrngImpl<Blake2s256>;

}  // namespace starkware
