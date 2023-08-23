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

#ifndef STARKWARE_CRYPT_TOOLS_MASKED_HASH_H_
#define STARKWARE_CRYPT_TOOLS_MASKED_HASH_H_

#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/crypt_tools/utils.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

/*
  Reduces the given hash, HashT, to be of size NumEffectiveBytes bytes. If IsMsb is true, it uses
  the most significant bytes, otherwise, the least significant bytes.
*/
template <typename HashT, size_t NumEffectiveBytes, bool IsMsb>
class MaskedHash {
 public:
  static constexpr size_t kDigestNumBytes = HashT::kDigestNumBytes;
  static_assert(NumEffectiveBytes <= kDigestNumBytes);
  static constexpr size_t kNumEffectiveBytes = NumEffectiveBytes;
  static constexpr bool kIsMsb = IsMsb;

  static std::string HashName() {
    std::string hash_name = HashT::HashName() + "_masked" + std::to_string(NumEffectiveBytes * 8);
    return kIsMsb ? hash_name + "_msb" : hash_name + "_lsb";
  }

  // In order to reduce Merkle initialization time, we don't want the digest to be initialized.
  // Therefore we replace the default constructor with one that doesn't.
  MaskedHash() {}  // NOLINT
  /*
    Gets a MaskedHash instance with the specified digest.
  */
  static MaskedHash InitDigestTo(const gsl::span<const std::byte>& digest) {
    // note: we don't erase LSB's even if they are set.
    return MaskedHash(digest);
  }

  static MaskedHash Hash(const MaskedHash& val1, const MaskedHash& val2) {
    return MaskHash(
        HashT::Hash(HashT::InitDigestTo(val1.GetDigest()), HashT::InitDigestTo(val2.GetDigest())));
  }

  static MaskedHash HashBytesWithLength(gsl::span<const std::byte> bytes) {
    return MaskHash(HashT::HashBytesWithLength(bytes));
  }

  static MaskedHash HashBytesWithLength(
      gsl::span<const std::byte> bytes, const MaskedHash& initial_hash) {
    return MaskHash(
        HashT::HashBytesWithLength(bytes, HashT::InitDigestTo(initial_hash.GetDigest())));
  }

  bool operator==(const MaskedHash& other) const { return buffer_ == other.buffer_; }
  bool operator!=(const MaskedHash& other) const { return buffer_ != other.buffer_; }
  const std::array<std::byte, kDigestNumBytes>& GetDigest() const { return buffer_; }
  std::string ToString() const {
    return kIsMsb ? BytesToHexString(buffer_) : BytesToHexString(buffer_, false);
  }
  friend std::ostream& operator<<(std::ostream& out, const MaskedHash& hash) {
    return out << hash.ToString();
  }

 private:
  /*
    Constructs a MaskedHash object based on the given data.
  */
  explicit MaskedHash(const gsl::span<const std::byte>& data)
      : buffer_(InitDigestFromSpan<MaskedHash>(data)) {}

  static MaskedHash MaskHash(const HashT& hash) {
    std::array<std::byte, kDigestNumBytes> bytes = {};
    if constexpr (IsMsb) {  // NOLINT: clang tidy if constexpr bug.
      std::copy_n(hash.GetDigest().begin(), kNumEffectiveBytes, bytes.begin());
    } else {  // NOLINT: clang tidy if constexpr bug.
      constexpr size_t kOffset = kDigestNumBytes - kNumEffectiveBytes;
      std::copy_n(hash.GetDigest().begin() + kOffset, kNumEffectiveBytes, bytes.begin() + kOffset);
    }
    return MaskedHash(bytes);
  }

  std::array<std::byte, kDigestNumBytes> buffer_;  // NOLINT
};

}  // namespace starkware

#endif  // STARKWARE_CRYPT_TOOLS_MASKED_HASH_H_
