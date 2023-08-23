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

#ifndef STARKWARE_CRYPT_TOOLS_BLAKE2S_H_
#define STARKWARE_CRYPT_TOOLS_BLAKE2S_H_

#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

template <size_t DigestNumBits>
class Blake2s {
 public:
  static constexpr size_t kDigestNumBytes = DigestNumBits / 8;

  static std::string HashName() { return "blake" + std::to_string(DigestNumBits); }

  // In order to reduce Merkle initialization time, we don't want the digest to be initialized.
  // Therefore we replace the default constructor with one that doesn't.
  Blake2s() {}  // NOLINT
  /*
    Gets a Blake2s instance with the specified digest.
  */
  static const Blake2s InitDigestTo(const gsl::span<const std::byte>& digest);

  // RVO (Return Value Optimization) assumed for this interface to be used efficiently.
  static const Blake2s HashWithoutFinalize(const Blake2s& val1, const Blake2s& val2);

  static const Blake2s Hash(const Blake2s& val1, const Blake2s& val2);

  static const Blake2s HashBytesWithLength(
      gsl::span<const std::byte> bytes, const Blake2s& initial_hash);

  static const Blake2s HashBytesWithLength(gsl::span<const std::byte> bytes);

  bool operator==(const Blake2s& other) const;
  bool operator!=(const Blake2s& other) const;
  const std::array<std::byte, kDigestNumBytes>& GetDigest() const { return buffer_; }
  std::string ToString() const;

  template <size_t DNB>
  friend std::ostream& operator<<(std::ostream& out, const Blake2s<DNB>& hash);

 private:
  static constexpr size_t kDigestNumDWords = kDigestNumBytes / sizeof(uint32_t);

  /*
    Constructs a Blake2s object based on the given data (assumed to be of size 32 bytes).
  */
  explicit Blake2s(const gsl::span<const std::byte>& data);

  std::array<std::byte, kDigestNumBytes> buffer_;  // NOLINT
};

using Blake2s160 = Blake2s<160>;
using Blake2s256 = Blake2s<256>;

}  // namespace starkware

#include "starkware/crypt_tools/blake2s.inl"

#endif  // STARKWARE_CRYPT_TOOLS_BLAKE2S_H_
