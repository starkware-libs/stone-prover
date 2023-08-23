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

#include "gtest/gtest.h"

namespace starkware {
namespace {

TEST(LongFieldElement, ToStandardForm) {
  ASSERT_EQ(LongFieldElement::FromUint(0).ToStandardForm(), BigInt<1>(0));
  ASSERT_EQ(
      (LongFieldElement::FromUint(10) + LongFieldElement::FromUint(103)).ToStandardForm(),
      BigInt<1>(113));
}

TEST(LongFieldElement, Constexpr) {
  constexpr LongFieldElement kV = LongFieldElement::FromUint(15);
  constexpr LongFieldElement kVInv = kV.Inverse();
  EXPECT_EQ(kV * kVInv, LongFieldElement::One());
}

TEST(LongFieldElement, kModulusBits) {
  constexpr uint64_t kMsb = Pow2(LongFieldElement::kModulusBits);
  constexpr uint64_t kUnusedMask = ~((kMsb << 1) - 1);

  static_assert((LongFieldElement::kModulus & kMsb) == kMsb, "msb should be set");
  static_assert((LongFieldElement::kModulus & kUnusedMask) == 0, "unused bits should be cleared");
}

TEST(LongFieldElement, FromInt) {
  EXPECT_EQ(LongFieldElement::FromInt(345), LongFieldElement::FromUint(345));
  EXPECT_EQ(LongFieldElement::FromInt(0), LongFieldElement::FromUint(0));
  EXPECT_EQ(
      LongFieldElement::FromInt(-20),
      LongFieldElement::FromUint(0) - LongFieldElement::FromUint(20));
  EXPECT_EQ(
      LongFieldElement::FromInt(-0x8000000000000000l),
      LongFieldElement::FromUint(0) - LongFieldElement::FromUint(0x8000000000000000l));
}

}  // namespace
}  // namespace starkware
