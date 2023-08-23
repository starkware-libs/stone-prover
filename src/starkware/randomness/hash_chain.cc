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

#include "starkware/randomness/hash_chain.h"

#include <algorithm>
#include <array>

#include "glog/logging.h"

#include "starkware/crypt_tools/template_instantiation.h"
#include "starkware/crypt_tools/utils.h"
#include "starkware/utils/serialization.h"

namespace starkware {

template <typename HashT>
HashChain<HashT>::HashChain() {
  hash_ = HashT::InitDigestTo(std::array<std::byte, HashT::kDigestNumBytes>{});
}

template <typename HashT>
HashChain<HashT>::HashChain(const gsl::span<const std::byte> public_input_data) {
  InitHashChain(public_input_data);
}

template <typename HashT>
void HashChain<HashT>::InitHashChain(const gsl::span<const std::byte> bytes) {
  hash_ = HashT::HashBytesWithLength(bytes);
  num_spare_bytes_ = 0;
  counter_ = 0;
}

template <typename HashT>
void HashChain<HashT>::GetRandomBytes(gsl::span<std::byte> random_bytes_out) {
  size_t num_bytes = random_bytes_out.size();
  size_t num_full_blocks = num_bytes / HashT::kDigestNumBytes;

  for (size_t offset = 0; offset < num_full_blocks * HashT::kDigestNumBytes;
       offset += HashT::kDigestNumBytes) {
    GetMoreRandomBytesUsingHashWithCounter(
        counter_++, random_bytes_out.subspan(offset, HashT::kDigestNumBytes));
  }

  size_t num_tail_bytes = num_bytes % HashT::kDigestNumBytes;
  if (num_tail_bytes <= num_spare_bytes_) {
    std::copy(
        spare_bytes_.begin(), spare_bytes_.begin() + num_tail_bytes,
        random_bytes_out.begin() + num_full_blocks * HashT::kDigestNumBytes);
    num_spare_bytes_ -= num_tail_bytes;
    std::copy(spare_bytes_.begin() + num_tail_bytes, spare_bytes_.end(), spare_bytes_.begin());
  } else {
    GetMoreRandomBytesUsingHashWithCounter(
        counter_++,
        random_bytes_out.subspan(num_full_blocks * HashT::kDigestNumBytes, num_tail_bytes));
  }
}

template <typename HashT>
void HashChain<HashT>::UpdateHashChain(const gsl::span<const std::byte> raw_bytes) {
  // UpdateHashChain is implemented using MixSeedWithBytes rather than the other way around
  // because MixSeedWithBytes is more performance critical.
  const uint64_t seed_increment = 0;
  this->MixSeedWithBytes(raw_bytes, seed_increment);
}

template <typename HashT>
void HashChain<HashT>::MixSeedWithBytes(
    const gsl::span<const std::byte> raw_bytes, const uint64_t seed_increment) {
  static_assert(HashT::kDigestNumBytes == BigInt<4>::SizeInBytes());

  std::vector<std::byte> mixed_bytes(HashT::kDigestNumBytes + raw_bytes.size());
  auto big_int = Deserialize<BigInt<4>>(hash_.GetDigest());
  big_int += BigInt<4>(seed_increment);
  Serialize<BigInt<4>>(big_int, gsl::span<std::byte>(mixed_bytes.data(), HashT::kDigestNumBytes));
  std::copy(raw_bytes.begin(), raw_bytes.end(), mixed_bytes.begin() + HashT::kDigestNumBytes);

  hash_ = HashT::HashBytesWithLength(mixed_bytes);
  num_spare_bytes_ = 0;
  counter_ = 0;
}

template <typename HashT>
void HashChain<HashT>::GetMoreRandomBytesUsingHashWithCounter(
    const uint64_t counter, gsl::span<std::byte> random_bytes_out) {
  size_t num_bytes = random_bytes_out.size();
  ASSERT_RELEASE(
      num_bytes <= HashT::kDigestNumBytes, "Asked to get more bytes than one digest size");
  auto prandom_bytes = HashWithCounter(hash_, counter).GetDigest();
  std::copy(prandom_bytes.begin(), prandom_bytes.begin() + num_bytes, random_bytes_out.begin());
  ASSERT_RELEASE(
      num_spare_bytes_ < HashT::kDigestNumBytes + num_bytes,
      "Not enough room in spare bytes buffer. Have " + std::to_string(num_spare_bytes_) +
          " bytes and want to add " + std::to_string(HashT::kDigestNumBytes - num_bytes) +
          " bytes");
  std::copy(
      prandom_bytes.begin() + num_bytes, prandom_bytes.end(),
      spare_bytes_.begin() + num_spare_bytes_);
  num_spare_bytes_ += (HashT::kDigestNumBytes - num_bytes);
}

template <typename HashT>
HashT HashChain<HashT>::HashWithCounter(const HashT& hash, uint64_t counter) {
  std::array<std::byte, 2 * HashT::kDigestNumBytes> data{};
  std::array<std::byte, sizeof(uint64_t)> bytes{};
  Serialize<uint64_t>(counter, bytes);

  ASSERT_RELEASE(
      sizeof(uint64_t) <= HashT::kDigestNumBytes,
      "Digest size must be larger than sizeof(uint64_t).");

  std::copy(hash.GetDigest().begin(), hash.GetDigest().end(), data.begin());
  // Copy the counter's serialized 64bit onto the MSB end of the buffer (PR #875 decision).
  std::copy(bytes.begin(), bytes.end(), data.end() - sizeof(uint64_t));
  return HashT::HashBytesWithLength(data);
}

INSTANTIATE_FOR_ALL_HASH_FUNCTIONS(HashChain);

}  // namespace starkware
