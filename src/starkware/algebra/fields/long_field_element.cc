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

#include "starkware/algebra/fields/long_field_element.h"

#include <cstddef>
#include <vector>

#include "starkware/utils/to_from_string.h"

namespace starkware {

void LongFieldElement::ToBytes(gsl::span<std::byte> span_out, bool use_big_endian) const {
  ASSERT_RELEASE(
      span_out.size() == SizeInBytes(), "Destination span size mismatches field element size.");
  return BigInt<1>(value_).ToBytes(span_out, use_big_endian);
}

LongFieldElement LongFieldElement::FromBytes(
    gsl::span<const std::byte> bytes, bool use_big_endian) {
  ASSERT_RELEASE(
      bytes.size() == SizeInBytes(), "Source span size mismatches field element size, expected " +
                                         std::to_string(SizeInBytes()) + ", got " +
                                         std::to_string(bytes.size()));
  return LongFieldElement(BigInt<1>::FromBytes(bytes, use_big_endian)[0]);
}

LongFieldElement LongFieldElement::FromString(const std::string& s) {
  std::array<std::byte, SizeInBytes()> as_bytes{};
  HexStringToBytes(s, as_bytes);
  uint64_t res = LongFieldElement::MontgomeryMul(
      Deserialize<uint64_t>(as_bytes, /*use_big_endian=*/true),
      LongFieldElement::kMontgomeryRSquared);
  return LongFieldElement(res);
}

std::string LongFieldElement::ToString() const {
  std::array<std::byte, BigInt<1>::SizeInBytes()> as_bytes{};
  Serialize(ToStandardForm(), as_bytes, /*use_big_endian=*/true);
  return BytesToHexString(as_bytes);
}

BigInt<1> LongFieldElement::ToStandardForm() const {
  return BigInt<1>::OneLimbMontgomeryReduction(
      BigInt<1>(value_), BigInt<1>(kModulus), kMontgomeryMPrime);
}

LongFieldElement LongFieldElement::RandomElement(PrngBase* prng) {
  // Note that we don't need to call FromUint here because skiping the call to
  // FromUint is the same as kMontgomeryR^-1 by which preserves the distribution.
  constexpr uint64_t kReleventBits = (Pow2(kModulusBits + 1)) - 1;

  std::array<std::byte, SizeInBytes()> bytes{};
  uint64_t deserialization;

  do {
    prng->GetRandomBytes(bytes);
    deserialization = Deserialize<uint64_t>(bytes) & kReleventBits;
  } while (deserialization >= kModulus);

  return LongFieldElement(deserialization);
}

}  // namespace starkware
