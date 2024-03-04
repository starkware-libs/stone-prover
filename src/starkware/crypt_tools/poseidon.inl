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

#include "third_party/poseidon/poseidon.h"

#include "starkware/crypt_tools/utils.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/to_from_string.h"

#include <vector>

namespace starkware {

extern "C" {
void permutation_3_montgomery(felt_t state_in_montgomery_form[]);
}

inline void FieldElementTToFeltT(FieldElementT cpp_felt, felt_t* res) {
  ValueType cpp_felt_value = cpp_felt.GetUnderlyingValueType();
  (*res)[0] = cpp_felt_value[0];
  (*res)[1] = cpp_felt_value[1];
  (*res)[2] = cpp_felt_value[2];
  (*res)[3] = cpp_felt_value[3];
  return;
}

inline FieldElementT FeltTToFieldElementT(felt_t c_felt) {
  return FieldElementT::FromMontgomeryForm(ValueType({c_felt[0], c_felt[1], c_felt[2], c_felt[3]}));
}

inline Poseidon3 Poseidon3::InitDigestTo(const gsl::span<const std::byte>& digest) {
  return Poseidon3(digest);
}

inline Poseidon3 Poseidon3::InitDigestTo(const FieldElementT& fieldElementDigest) {
  return Poseidon3(fieldElementDigest);
}

inline Poseidon3 Poseidon3::Hash(const Poseidon3& val0, const Poseidon3& val1) {
  std::array<felt_t, 3> c_state_buffer = {};

  FieldElementTToFeltT(val0.state_, &c_state_buffer[0]);
  FieldElementTToFeltT(val1.state_, &c_state_buffer[1]);
  FieldElementTToFeltT(FieldElementT::FromBigInt(BigInt<4>(2)), &c_state_buffer[2]);

  // Perform permutation.
  permutation_3_montgomery(c_state_buffer.data());

  // Return a new poseidon3 instance.
  return InitDigestTo(FeltTToFieldElementT(c_state_buffer[0]));
}

inline FieldElementT Poseidon3::Hash(const FieldElementT& val0, const FieldElementT& val1) {
  std::array<felt_t, 3> c_state_buffer = {};

  FieldElementTToFeltT(val0, &c_state_buffer[0]);
  FieldElementTToFeltT(val1, &c_state_buffer[1]);
  FieldElementTToFeltT(FieldElementT::FromBigInt(BigInt<4>(2)), &c_state_buffer[2]);

  // Perform permutation.
  permutation_3_montgomery(c_state_buffer.data());

  // Return the hash digest.
  return FeltTToFieldElementT(c_state_buffer[0]);
}

inline Poseidon3 Poseidon3::HashBytesWithLength(gsl::span<const std::byte> bytes) {
  ASSERT_VERIFIER(bytes.size() % kFieldElementNumBytes == 0, "Bad input legnth.");

  std::array<felt_t, 3> c_state_buffer = {};
  int n_digest = bytes.size() / kFieldElementNumBytes;

  // Sponge construction.
  for (int digested_idx = 0; digested_idx <= n_digest; digested_idx += 2) {
    switch (n_digest - digested_idx) {
      case 0:
        FieldElementTToFeltT(
            FeltTToFieldElementT(c_state_buffer[0]) + FieldElementT::One(), &c_state_buffer[0]);
        break;
      case 1:
        FieldElementTToFeltT(
            FeltTToFieldElementT(c_state_buffer[0]) + SpanToFieldElementT(bytes, digested_idx),
            &c_state_buffer[0]);
        FieldElementTToFeltT(
            FeltTToFieldElementT(c_state_buffer[1]) + FieldElementT::One(), &c_state_buffer[1]);
        break;
      default:
        FieldElementTToFeltT(
            FeltTToFieldElementT(c_state_buffer[0]) + SpanToFieldElementT(bytes, digested_idx),
            &c_state_buffer[0]);
        FieldElementTToFeltT(
            FeltTToFieldElementT(c_state_buffer[1]) + SpanToFieldElementT(bytes, digested_idx + 1),
            &c_state_buffer[1]);
    }
    // Perform permutation.
    permutation_3_montgomery(c_state_buffer.data());
  }

  return InitDigestTo(FeltTToFieldElementT(c_state_buffer[0]));
}

inline FieldElementT Poseidon3::HashFeltsWithLength(gsl::span<const FieldElementT> field_elements) {
  std::array<felt_t, 3> c_state_buffer = {};
  int n_digest = field_elements.size();

  // Sponge construction.
  for (int digested_idx = 0; digested_idx <= n_digest; digested_idx += 2) {
    switch (n_digest - digested_idx) {
      case 0:
        FieldElementTToFeltT(
            FeltTToFieldElementT(c_state_buffer[0]) + FieldElementT::One(), &c_state_buffer[0]);
        break;
      case 1:
        FieldElementTToFeltT(
            FeltTToFieldElementT(c_state_buffer[0]) + field_elements[digested_idx],
            &c_state_buffer[0]);
        FieldElementTToFeltT(
            FeltTToFieldElementT(c_state_buffer[1]) + FieldElementT::One(), &c_state_buffer[1]);
        break;
      default:
        FieldElementTToFeltT(
            FeltTToFieldElementT(c_state_buffer[0]) + field_elements[digested_idx],
            &c_state_buffer[0]);
        FieldElementTToFeltT(
            FeltTToFieldElementT(c_state_buffer[1]) + field_elements[digested_idx + 1],
            &c_state_buffer[1]);
    }
    // Perform permutation.
    permutation_3_montgomery(c_state_buffer.data());
  }

  // Return a new poseidon3 instance.
  return FeltTToFieldElementT(c_state_buffer[0]);
}

inline FieldElementT Poseidon3::SpanToFieldElementT(
    gsl::span<const std::byte> spn, int start_idx, bool use_big_endian) {
  ValueType element = ValueType::FromBytes(
      spn.subspan(start_idx * kDigestNumBytes, kDigestNumBytes), use_big_endian);
  ASSERT_RELEASE(
      element < FieldElementT::GetModulus(), "The input must be smaller than the field prime.");
  return FieldElementT::FromBigInt(element);
}

inline bool Poseidon3::operator==(const Poseidon3& other) const { return state_ == other.state_; }
inline bool Poseidon3::operator!=(const Poseidon3& other) const { return state_ != other.state_; }

inline std::string Poseidon3::ToString() const { return BytesToHexString(GetDigest()); }

inline std::ostream& operator<<(std::ostream& out, const Poseidon3& hash) {
  return out << hash.ToString();
}

inline Poseidon3::Poseidon3(const gsl::span<const std::byte>& bytes)
    : state_(SpanToFieldElementT(bytes, 0)) {}

}  // namespace starkware
