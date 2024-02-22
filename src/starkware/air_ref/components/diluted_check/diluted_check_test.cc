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

#include "starkware/air/components/diluted_check/diluted_check.h"

#include <memory>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/components/diluted_check/diluted_check_cell.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using FieldElementT = TestFieldElement;

/*
  Tests that ExpectedFinalCumulativeValue() computes the correct value.
*/
TEST(DilutedCheckTest, ExpectedFinalCumulativeValue) {
  Prng prng;
  const auto spacing = prng.UniformInt<size_t>(1, 7);
  const auto n_bits = prng.UniformInt<size_t>(1, 8);
  const auto alpha = FieldElementT::RandomElement(&prng);
  const auto z = FieldElementT::RandomElement(&prng);

  FieldElementT r = FieldElementT::One();
  for (uint64_t i = 1; i < uint64_t(1) << n_bits; ++i) {
    const auto d = FieldElementT::FromUint(
        diluted_check_cell::Dilute(i, spacing, n_bits) -
        diluted_check_cell::Dilute(i - 1, spacing, n_bits));
    r = r * (FieldElementT::One() + z * d) + alpha * d * d;
  }

  ASSERT_EQ(
      r, DilutedCheckComponentProverContext1<FieldElementT>::ExpectedFinalCumulativeValue(
             spacing, n_bits, z, alpha));
}

}  // namespace
}  // namespace starkware
