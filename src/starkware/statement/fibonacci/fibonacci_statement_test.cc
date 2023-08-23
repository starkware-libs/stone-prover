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

#include "starkware/statement/fibonacci/fibonacci_statement.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/air/air.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

class FibonacciStatementTest : public testing::Test {
 public:
  JsonValue public_input = JsonValue::FromString(R"(
  {
    "fibonacci_claim_index": 5,
    "claimed_fib": "0x6d13fea0e5c574015d84e9f9d869601289a09f1d6e7a4ca15b064f5b69a4d4a"
  }
  )");
};

/*
  Tests that the code compiles and doesn't throw any runtime exceptions.
*/
TEST_F(FibonacciStatementTest, BasicTest) {
  using FieldElementT = PrimeFieldElement<252, 0>;

  auto private_input = JsonValue::FromString(R"(
  {
    "witness": "0x1234"
  }
  )");
  FieldElementT witness = private_input["witness"].template AsFieldElement<FieldElementT>();
  // Expected Fibonacci expansion: 1, x, x+1, 2x+1, 3x+2, 5x+3   <-- index 5.
  FieldElementT expected_result = witness * FieldElementT::FromUint(5) + FieldElementT::FromUint(3);
  FibonacciStatement<FieldElementT> statement(public_input, private_input);
  // Check the type of the returned AIR.
  EXPECT_NE(dynamic_cast<const FibonacciAir<FieldElementT>*>(&statement.GetAir()), nullptr);
  statement.GetInitialHashChainSeed();
  statement.GetTraceContext()->GetTrace();
  JsonValue fixed_public_input = statement.FixPublicInput();
  EXPECT_EQ(
      fixed_public_input["claimed_fib"].template AsFieldElement<FieldElementT>(), expected_result);
}

TEST_F(FibonacciStatementTest, PartialPublicTest) {
  using FieldElementT = PrimeFieldElement<252, 0>;

  JsonValue public_input_partial = JsonValue::FromString(R"(
  {
    "fibonacci_claim_index": 5
  }
  )");

  auto private_input = JsonValue::FromString(R"(
  {
    "witness": "0x1234"
  }
  )");
  FieldElementT witness = private_input["witness"].template AsFieldElement<FieldElementT>();
  // Expected Fibonacci expansion: 1, x, x+1, 2x+1, 3x+2, 5x+3   <-- index 5.
  FieldElementT expected_result = witness * FieldElementT::FromUint(5) + FieldElementT::FromUint(3);
  FibonacciStatement<FieldElementT> statement(public_input_partial, private_input);

  // Check we get right exception.
  EXPECT_ASSERT(statement.GetAir(), testing::HasSubstr("claimed Fibonacci value not set"));
  EXPECT_ASSERT(
      statement.GetInitialHashChainSeed(), testing::HasSubstr("claimed Fibonacci value not set"));
  EXPECT_ASSERT(statement.GetTraceContext()->GetTrace(), testing::HasSubstr("GetAir()"));

  JsonValue fixed_public_input = statement.FixPublicInput();

  // Check the type of the returned AIR.
  EXPECT_NE(dynamic_cast<const FibonacciAir<FieldElementT>*>(&statement.GetAir()), nullptr);
  statement.GetInitialHashChainSeed();
  statement.GetTraceContext()->GetTrace();
  EXPECT_EQ(
      fixed_public_input["claimed_fib"].template AsFieldElement<FieldElementT>(), expected_result);
}

}  // namespace
}  // namespace starkware
