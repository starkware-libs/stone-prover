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

#include "starkware/stark/stark.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/degree_three_example/degree_three_example_air.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

/*
  Tests that stark params throws an error when creating starkware params with use_extension_field =
  true and a field which is not in the form of ExtensionFieldElement<FieldElementT>.
*/
TEST(StarkParams, UseExtensionFieldTest) {
  using FieldElementT = TestFieldElement;
  using DegThreeAirT = DegreeThreeExampleAir<FieldElementT>;
  Prng prng;
  const auto trace_length = 256;
  const uint64_t res_claim_index = 251;
  const Field field(Field::Create<FieldElementT>());
  const auto secret = FieldElementT::RandomElement(&prng);
  const auto air = DegThreeAirT(
      trace_length, res_claim_index,
      DegThreeAirT::PublicInputFromPrivateInput(secret, res_claim_index));

  EXPECT_ASSERT(
      StarkParameters(
          field, /*use_extension_field=*/true, 8, trace_length, UseOwned(&air), nullptr),
      testing::HasSubstr(
          "Use extension field is true but the field is not of type extension field."));
}

}  // namespace
}  // namespace starkware
