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

#ifndef STARKWARE_CRYPT_TOOLS_KECCAK_256_H_
#define STARKWARE_CRYPT_TOOLS_KECCAK_256_H_

#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

class Keccak256 {
 public:
  static constexpr size_t kDigestNumBytes = 256 / 8;
  static constexpr size_t kStateNumBytes = 5 * 5 * 64 / 8;
  static std::string HashName() { return "keccak256"; }

  // In order to reduce Merkle initialization time, we don't want the digest to be initialized.
  // Therefore we replace the default constructor with one that doesn't.
  Keccak256() {}  // NOLINT
  /*
    Gets a Keccak256 instance with the specified digest.
  */
  static Keccak256 InitDigestTo(const gsl::span<const std::byte>& digest);

  static Keccak256 Hash(const Keccak256& val1, const Keccak256& val2);

  static Keccak256 HashBytesWithLength(gsl::span<const std::byte> bytes);

  static std::array<std::byte, kStateNumBytes> ApplyPermutation(gsl::span<const std::byte> bytes);

  static Keccak256 HashBytesWithLength(
      gsl::span<const std::byte> bytes, const Keccak256& initial_hash);

  bool operator==(const Keccak256& other) const;
  bool operator!=(const Keccak256& other) const;
  const std::array<std::byte, kDigestNumBytes>& GetDigest() const { return buffer_; }
  std::string ToString() const;
  friend std::ostream& operator<<(std::ostream& out, const Keccak256& sha);

 private:
  class keccak_state;
  /*
    Constructs a Keccak256 object based on the given data (assumed to be of size 32 bytes).
  */
  explicit Keccak256(const gsl::span<const std::byte>& data);

  std::array<std::byte, kDigestNumBytes> buffer_;
};

}  // namespace starkware

#include "starkware/crypt_tools/keccak_256.inl"

#endif  // STARKWARE_CRYPT_TOOLS_KECCAK_256_H_
