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

#include "starkware/algebra/fields/prime_field_element.h"

#include <limits>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "gtest/gtest.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {
namespace {

template <typename T>
class PrimeFieldElementTest : public ::testing::Test {};

using TestedPrimeFETypes = ::testing::Types<
    PrimeFieldElement<252, 0>, PrimeFieldElement<254, 1>, PrimeFieldElement<254, 2>,
    PrimeFieldElement<252, 3>, PrimeFieldElement<255, 4>, PrimeFieldElement<124, 5>>;

TYPED_TEST_CASE(PrimeFieldElementTest, TestedPrimeFETypes);

TYPED_TEST(PrimeFieldElementTest, One) {
  auto a = TypeParam::One();
  EXPECT_EQ(a * a, a);
}

TYPED_TEST(PrimeFieldElementTest, OneNegTest) {
  auto a = TypeParam::One();
  EXPECT_NE(a * a, a + a);
}

TYPED_TEST(PrimeFieldElementTest, AddOneToMinusOne) {
  auto a = TypeParam::One();
  auto z = TypeParam::Zero();
  auto b = z - a;
  EXPECT_EQ(a + b, z);
}

TYPED_TEST(PrimeFieldElementTest, AddSimple) {
  if constexpr (TypeParam::ValueType::kN >= 4) {  // NOLINT: clang-tidy if constexpr bug.
    const auto a = TypeParam::FromMontgomeryForm(
        0x01f5883e65f820d099915c908686b9d1c903896a679f32d65369cbe3b0000005_Z);
    const auto b = TypeParam::FromMontgomeryForm(
        0x010644e72e131a0f9b8504b68181585d94816a916871ca8d3c208c16d87cfd42_Z);
    const auto z = TypeParam::FromMontgomeryForm(
        0x02fbcd25940b3ae0351661470808122f5d84f3fbd010fd638f8a57fa887cfd47_Z);
    EXPECT_EQ(a + b, z);
  }
}

TYPED_TEST(PrimeFieldElementTest, Inv) {
  auto a = TypeParam::One();
  auto z = TypeParam::Zero();
  auto b = z - a;

  EXPECT_EQ(b.Inverse(), b);
}

TYPED_TEST(PrimeFieldElementTest, FromInt) {
  static constexpr size_t kN = TypeParam::ValueType::kN;
  Prng prng;
  TypeParam a = TypeParam::FromUint(1);
  EXPECT_EQ(a, TypeParam::One());
  TypeParam b = TypeParam::FromBigInt(BigInt<kN>::One());
  EXPECT_EQ(b, TypeParam::One());
  static constexpr TypeParam kOne = TypeParam::ConstexprFromBigInt(BigInt<kN>::One());
  EXPECT_EQ(kOne, TypeParam::One());

  const TypeParam two = TypeParam::One() + TypeParam::One();
  TypeParam expected_res = TypeParam::Zero();
  std::array<uint64_t, kN> arr{};
  for (size_t i = 0; i < kN; ++i) {
    const uint64_t num = prng.UniformInt<uint64_t>(0, std::numeric_limits<uint64_t>::max() - 1);
    arr.at(i) = num;
    expected_res += TypeParam::FromUint(num) * Pow(two, 64 * i);
  }
  TypeParam c = TypeParam::FromBigInt(BigInt<kN>(arr));
  EXPECT_EQ(c, expected_res);
}

TEST(PrimeField, ToStandardForm) {
  using ValueType = PrimeFieldElement<252, 0>::ValueType;
  Prng prng;
  ValueType val = ValueType::RandomBigInt(&prng);
  auto montgomery_form = PrimeFieldElement<252, 0>::FromBigInt(val);
  std::pair<ValueType, ValueType> quotient_remainder =
      ValueType::Div(val, PrimeFieldElement<252, 0>::GetModulus());
  EXPECT_EQ(quotient_remainder.second, montgomery_form.ToStandardForm());
}

TEST(PrimeField, Random) {
  Prng prng(MakeByteArray<0xca, 0xfe, 0xca, 0xfe>());
  for (size_t i = 0; i < 100; ++i) {
    auto a = PrimeFieldElement<252, 0>::RandomElement(&prng);
    auto b = PrimeFieldElement<252, 0>::RandomElement(&prng);
    EXPECT_NE(a, b);
  }
}

}  // namespace
}  // namespace starkware
