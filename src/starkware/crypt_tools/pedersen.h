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

#ifndef STARKWARE_CRYPT_TOOLS_PEDERSEN_H_
#define STARKWARE_CRYPT_TOOLS_PEDERSEN_H_

#include <array>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>

#include "starkware/algebra/fields/prime_field_element.h"

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

using FieldElementT = PrimeFieldElement<252, 0>;
using ValueType = FieldElementT::ValueType;

// Note: This hash is collision resistant, but not known to have other pseudo random qualities in
// general.
class Pedersen {
 public:
  static constexpr size_t kDigestNumBytes = SafeDiv(256, 8);
  static std::string HashName() { return "pedersen"; }

  Pedersen() : state_(FieldElementT::Uninitialized()) {}  // NOLINT

  static Pedersen InitDigestTo(const gsl::span<const std::byte>& digest);

  static Pedersen Hash(const Pedersen& val0, const Pedersen& val1);

  static Pedersen HashBytesWithLength(gsl::span<const std::byte> bytes);

  static Pedersen HashBytesWithLength(
      gsl::span<const std::byte> bytes, const Pedersen& initial_hash);

  bool operator==(const Pedersen& other) const;
  bool operator!=(const Pedersen& other) const;

  const std::array<std::byte, kDigestNumBytes> GetDigest() const {
    std::array<std::byte, kDigestNumBytes> output{};
    state_.ToStandardForm().ToBytes(output);
    return output;
  }

  std::string ToString() const;
  friend std::ostream& operator<<(std::ostream& out, const Pedersen& hash);

 private:
  explicit Pedersen(const FieldElementT& state) : state_(state) {}

  FieldElementT state_;
};

}  // namespace starkware

#include "starkware/crypt_tools/pedersen.inl"

#endif  // STARKWARE_CRYPT_TOOLS_PEDERSEN_H_
