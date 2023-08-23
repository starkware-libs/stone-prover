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

#include "starkware/crypt_tools/pedersen.h"

#include "starkware/crypt_tools/hash_context/pedersen_hash_context.h"
#include "starkware/crypt_tools/utils.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

inline Pedersen Pedersen::InitDigestTo(const gsl::span<const std::byte>& digest) {
  return Pedersen(FieldElementT::FromBigInt(ValueType::FromBytes(digest)));
}

inline Pedersen Pedersen::Hash(const Pedersen& val0, const Pedersen& val1) {
  const auto& ctx = GetStandardPedersenHashContext();

  const auto res = ctx.Hash(val0.state_, val1.state_);
  return Pedersen(res);
}

inline Pedersen Pedersen::HashBytesWithLength(gsl::span<const std::byte> bytes) {
  return Pedersen::HashBytesWithLength(bytes, Pedersen(FieldElementT::Zero()));
}

inline Pedersen Pedersen::HashBytesWithLength(
    gsl::span<const std::byte> bytes, const Pedersen& initial_hash) {
  FieldElementT state = initial_hash.state_;
  const auto& ctx = GetStandardPedersenHashContext();
  const auto modulus = FieldElementT::GetModulus();

  size_t bytes_to_hash = bytes.size();
  size_t offset = 0;

  // Absorb full block of kDigestNumBytes bytes.
  while (bytes_to_hash >= kDigestNumBytes) {
    const ValueType word = ValueType::FromBytes(bytes.subspan(offset, kDigestNumBytes));
    const auto [q, r] = word.Div(modulus);  // NOLINT: Structured binding syntax.
    const FieldElementT value = FieldElementT::FromBigInt(r);
    ASSERT_RELEASE(q < ValueType(1000), "Unexpectedly large shift.");
    const FieldElementT shift = FieldElementT::FromBigInt(q);
    state = ctx.Hash(state, value) + shift;
    offset += kDigestNumBytes;
    bytes_to_hash -= kDigestNumBytes;
  }

  ASSERT_RELEASE(bytes_to_hash == 0, "Pedersen hash currently does not support partial blocks.");
  state = ctx.Hash(state, FieldElementT::FromUint(SafeDiv(bytes.size(), kDigestNumBytes)));

  return Pedersen(state);
}

inline std::string Pedersen::ToString() const { return state_.ToStandardForm().ToString(); }

inline std::ostream& operator<<(std::ostream& out, const Pedersen& hash) {
  return out << hash.ToString();
}

inline bool Pedersen::operator==(const Pedersen& other) const { return state_ == other.state_; }
inline bool Pedersen::operator!=(const Pedersen& other) const { return state_ != other.state_; }

}  // namespace starkware
