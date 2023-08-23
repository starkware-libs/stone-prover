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

#include "starkware/algebra/fields/test_field_element.h"

#include <algorithm>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/utils/serialization.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

void TestFieldElement::ToBytes(gsl::span<std::byte> span_out, bool use_big_endian) const {
  ASSERT_DEBUG(
      span_out.size() == SizeInBytes(), "Destination span size mismatches field element size.");

  Serialize(value_, span_out, use_big_endian);
}

TestFieldElement TestFieldElement::RandomElement(PrngBase* prng) {
  std::array<std::byte, SizeInBytes()> bytes{};
  uint32_t deserialization;
  do {
    prng->GetRandomBytes(bytes);
    deserialization = Deserialize<uint32_t>(bytes);
  } while (deserialization >= kModulus);
  return TestFieldElement(deserialization);
}

TestFieldElement TestFieldElement::FromBytes(
    const gsl::span<const std::byte> bytes, bool use_big_endian) {
  ASSERT_RELEASE(
      bytes.size() == SizeInBytes(), "Source span size mismatches field element size, expected " +
                                         std::to_string(SizeInBytes()) + ", got " +
                                         std::to_string(bytes.size()));

  return TestFieldElement(Deserialize<uint32_t>(bytes, use_big_endian));
}

TestFieldElement TestFieldElement::FromString(const std::string& s) {
  std::array<std::byte, SizeInBytes()> as_bytes{};
  HexStringToBytes(s, as_bytes);
  return TestFieldElement(Deserialize<uint32_t>(as_bytes, /*use_big_endian=*/true));
}

std::string TestFieldElement::ToString() const {
  std::array<std::byte, TestFieldElement::SizeInBytes()> as_bytes{};
  Serialize(value_, as_bytes, /*use_big_endian=*/true);
  return BytesToHexString(as_bytes);
}

}  // namespace starkware
