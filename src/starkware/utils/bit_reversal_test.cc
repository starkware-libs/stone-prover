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

#include "starkware/utils/bit_reversal.h"

#include <vector>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polymorphic/field_element_span.h"
#include "starkware/algebra/uint128.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::ElementsAre;

TEST(BitReverse, Integer) {
  EXPECT_EQ(BitReverse(0b1, 4), 0b1000U);
  EXPECT_EQ(BitReverse(0b1101, 4), 0b1011U);
  EXPECT_EQ(BitReverse(0b1101, 6), 0b101100U);

  EXPECT_EQ(BitReverse(0xffffffffefecc8e7L, 64), 0xe71337f7ffffffffL);
}

TEST(BitReverse, RandomInteger) {
  Prng prng;
  auto logn = prng.template UniformInt<size_t>(1, 63);
  auto val = prng.template UniformInt<uint64_t>(0, Pow2(logn) - 1);
  EXPECT_EQ(val, BitReverse(BitReverse(val, logn), logn));
}

TEST(BitReverse, InPlace) {
  std::vector<int> arr = {9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
  BitReverseInPlace(gsl::make_span(arr).subspan(1, 8));
  EXPECT_THAT(arr, ElementsAre(9, 10, 14, 12, 16, 11, 15, 13, 17, 18));
}

TEST(BitReverse, Vector) {
  std::vector<int> a;
  const size_t log_n = 3;
  const uint64_t n = Pow2(log_n);
  a.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    a.push_back(i);
  }
  auto a_rev = BitReverseVector(a);
  for (size_t i = 0; i < a.size(); ++i) {
    EXPECT_EQ(a[i], a_rev[BitReverse(i, log_n)]);
  }
}

TEST(BitReverse, FieldElementSpan) {
  const size_t log_n = 3;
  const uint64_t n = Pow2(log_n);
  FieldElementVector a =
      FieldElementVector::MakeUninitialized(Field::Create<TestFieldElement>(), n);
  FieldElementVector a_rev =
      FieldElementVector::MakeUninitialized(Field::Create<TestFieldElement>(), n);

  for (size_t i = 0; i < n; ++i) {
    a.Set(i, FieldElement(TestFieldElement::FromUint(i)));
  }
  BitReverseVector(a, a_rev);
  for (size_t i = 0; i < a.Size(); ++i) {
    EXPECT_EQ(a[i], a_rev[BitReverse(i, log_n)]);
  }
}

}  // namespace
}  // namespace starkware
