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

#ifndef STARKWARE_CRYPT_TOOLS_POSEIDON_H_
#define STARKWARE_CRYPT_TOOLS_POSEIDON_H_

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/error_handling/error_handling.h"
#include "third_party/poseidon/poseidon.h"

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

using FieldElementT = PrimeFieldElement<252, 0>;
using ValueType = FieldElementT::ValueType;

class Poseidon3 {
 public:
  static constexpr size_t kDigestNumBytes = FieldElementT::SizeInBytes();
  static constexpr size_t kStateNumBytes = 3 * FieldElementT::SizeInBytes();

  /*
    Gets the name of the poseidon instance, e.g. poseidon{x} is poseidon with state size x.
  */
  static std::string HashName() { return "poseidon3"; }

  /*
    In order to reduce Merkle initialization time, we don't want the digest to be initialized.
    Therefore we replace the default constructor with one that doesn't.
  */
  Poseidon3() {}

  /*
    Gets the hash digest, defined as the first FieldElementT in the internal state.
  */
  inline const std::array<std::byte, kDigestNumBytes> GetDigest() const {
    std::array<std::byte, kDigestNumBytes> digest_arr;
    auto digest_arr_span = gsl::make_span(digest_arr);
    state_.ToBytesStandardForm(digest_arr_span);
    return digest_arr;
  }

  /*
    Gets a digest and returns a Poseidon3 instance initialized with the specified digest.
  */
  static Poseidon3 InitDigestTo(const gsl::span<const std::byte>& digest);
  static Poseidon3 InitDigestTo(const FieldElementT& fieldElementDigest);

  /*
    Hash two elements. The function preforms:
    permute(S0=val0, S1=val1, S2=2).
  */
  static Poseidon3 Hash(const Poseidon3& val0, const Poseidon3& val1);
  static FieldElementT Hash(const FieldElementT& val0, const FieldElementT& val1);

  bool operator==(const Poseidon3& other) const;
  bool operator!=(const Poseidon3& other) const;

  /*
    Converts to string, returns the state_ in standard form.
  */
  std::string ToString() const;

  /*
    Outputs stream, returns the state_.
  */
  friend std::ostream& operator<<(std::ostream& out, const Poseidon3& hash);

  static Poseidon3 HashBytesWithLength([[maybe_unused]] gsl::span<const std::byte> bytes);

  static FieldElementT HashFeltsWithLength(
      [[maybe_unused]] gsl::span<const FieldElementT> field_elements);

  static Poseidon3 HashBytesWithLength(
      [[maybe_unused]] gsl::span<const std::byte> bytes,
      [[maybe_unused]] const Poseidon3& initial_hash) {
    ASSERT_RELEASE(false, "Not implemented");
    return Poseidon3();
  }

  static FieldElementT SpanToFieldElementT(
      gsl::span<const std::byte> spn, int start_idx, bool use_big_endian = true);

 private:
  static constexpr size_t kFieldElementNumBytes = FieldElementT::SizeInBytes();

  /*
    Constructs a Poseidon3 object with internal state initialized using FieldElementT in standard
    form.
  */
  explicit Poseidon3(const FieldElementT& state) { state_ = state; }

  /*
    Constructs a Poseidon3 object with internal state initialized with the provided array of bytes.
    The array must be the exact same length as FieldElementT.
  */
  explicit Poseidon3(const gsl::span<const std::byte>& bytes);

  // Represents the internal state.
  FieldElementT state_;
};

}  // namespace starkware

#include "starkware/crypt_tools/poseidon.inl"

#endif  // STARKWARE_CRYPT_TOOLS_POSEIDON_H_
