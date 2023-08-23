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

#include "starkware/algebra/field_operations.h"

#include <cstddef>
#include <vector>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/algebra/fields/fraction_field_element.h"
#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polymorphic/field.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::HasSubstr;

template <typename T>
class FieldAxiomTest : public ::testing::Test {
 public:
  // This allows each test to use a field f of type TypeParam, with a PRNG called prng.
  Field f;
  Prng prng;
  FieldAxiomTest() : f(Field::Create<T>()) {}
  // A convenience function to avoid the mess of "this->f.RandomElement(&this->prng)" every time.
  auto RandomElement() { return f.RandomElement(&prng); }
};

using TestedFieldTypes = ::testing::Types<
    TestFieldElement, LongFieldElement, PrimeFieldElement<252, 0>,
    FractionFieldElement<TestFieldElement>, FractionFieldElement<PrimeFieldElement<252, 0>>,
    ExtensionFieldElement<TestFieldElement>, ExtensionFieldElement<PrimeFieldElement<252, 0>>>;
TYPED_TEST_CASE(FieldAxiomTest, TestedFieldTypes);

template <typename T>
class AllFieldsTest : public ::testing::Test {};

using AllFieldTypes = ::testing::Types<
    TestFieldElement, LongFieldElement, PrimeFieldElement<252, 0>, PrimeFieldElement<254, 1>,
    PrimeFieldElement<254, 2>, PrimeFieldElement<252, 3>, PrimeFieldElement<255, 4>,
    PrimeFieldElement<124, 5>, FractionFieldElement<TestFieldElement>,
    FractionFieldElement<PrimeFieldElement<252, 0>>, ExtensionFieldElement<TestFieldElement>,
    ExtensionFieldElement<PrimeFieldElement<252, 0>>>;
TYPED_TEST_CASE(AllFieldsTest, AllFieldTypes);

template <typename T>
class PrimeFieldsTest : public ::testing::Test {};

using PrimeFieldTypes =
    ::testing::Types<TestFieldElement, LongFieldElement, PrimeFieldElement<252, 0>>;
TYPED_TEST_CASE(PrimeFieldsTest, PrimeFieldTypes);

// --- Test operators --- 2nd part:

// --- Test axioms ---

TYPED_TEST(FieldAxiomTest, OneIsNotZero) { EXPECT_NE(this->f.One(), this->f.Zero()); }

TYPED_TEST(FieldAxiomTest, AdditiveIdentity) {
  for (unsigned i = 0; i < 100; ++i) {
    auto r = this->RandomElement();
    ASSERT_EQ(r, r + this->f.Zero());
  }
}

TYPED_TEST(FieldAxiomTest, MultiplicativeIdentity) {
  for (unsigned i = 0; i < 100; ++i) {
    auto r = this->RandomElement();
    ASSERT_EQ(r, r * this->f.One());
  }
}

TYPED_TEST(FieldAxiomTest, AdditiveInverse) {
  for (unsigned i = 0; i < 100; ++i) {
    auto r = this->RandomElement();
    auto r_inverse = -r;
    ASSERT_EQ(this->f.Zero(), r + r_inverse);
  }
}

/*
  This is aimed to play a bit with -1, hopefully catching mistakes in implementations.
  We check that:
    1 + (-1) = 0
    (-1) * (-1) = 1
    1 / (-1) = (-1).
*/
TYPED_TEST(FieldAxiomTest, MinusOneTests) {
  auto minus_one = this->f.Zero() - this->f.One();
  EXPECT_EQ(this->f.Zero(), this->f.One() + minus_one);
  EXPECT_EQ(this->f.One(), minus_one * minus_one);
  EXPECT_EQ(minus_one, minus_one.Inverse());
}

TYPED_TEST(FieldAxiomTest, MultiplicativeInverse) {
  for (unsigned i = 0; i < 100; ++i) {
    auto r = this->RandomElement();
    if (r != this->f.Zero()) {
      auto r_inverse = r.Inverse();
      ASSERT_EQ(this->f.One(), r * r_inverse);
    }
  }
}

TYPED_TEST(FieldAxiomTest, AdditiveCommutativity) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomElement();
    auto b = this->RandomElement();
    ASSERT_EQ(a + b, b + a);
  }
}

TYPED_TEST(FieldAxiomTest, MultiplicativeCommutativity) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomElement();
    auto b = this->RandomElement();
    ASSERT_EQ(a * b, b * a);
  }
}

TYPED_TEST(FieldAxiomTest, AdditiveAssociativity) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomElement();
    auto b = this->RandomElement();
    auto c = this->RandomElement();
    auto d = a + b;
    d = d + c;
    auto e = b + c;
    e = a + e;
    ASSERT_EQ(d, e);
  }
}

TYPED_TEST(FieldAxiomTest, MultiplicativeAssociativity) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomElement();
    auto b = this->RandomElement();
    auto c = this->RandomElement();
    auto d = a * b;
    d = d * c;
    auto e = b * c;
    e = a * e;
    ASSERT_EQ(d, e);
  }
}

TYPED_TEST(FieldAxiomTest, Distributivity) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomElement();
    auto b = this->RandomElement();
    auto c = this->RandomElement();
    auto d = a * (b + c);
    auto e = a * b;
    auto f = a * c;
    e = e + f;
    ASSERT_EQ(d, e);
  }
}

TYPED_TEST(FieldAxiomTest, ToFromBytes) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomElement();
    std::vector<std::byte> a_bytes(a.SizeInBytes());
    a.ToBytes(a_bytes);
    auto b = this->f.FromBytes(a_bytes);
    ASSERT_EQ(a, b);
  }
}

TYPED_TEST(FieldAxiomTest, ToFromString) {
  for (unsigned i = 0; i < 100; ++i) {
    std::stringstream s;
    auto a = this->RandomElement();
    s << a;
    auto b = this->f.FromString(s.str());
    ASSERT_EQ(a, b);
  }
}

TYPED_TEST(FieldAxiomTest, BatchInverse) {
  using FieldElementT = TypeParam;
  const size_t size = this->prng.UniformInt(0, 10);
  std::vector<FieldElementT> input(size, FieldElementT::Zero());
  std::vector<FieldElementT> expected_output(size, FieldElementT::Zero());
  std::vector<FieldElementT> output(size, FieldElementT::Zero());
  for (size_t i = 0; i < size; ++i) {
    const FieldElementT e = RandomNonZeroElement<FieldElementT>(&this->prng);
    input[i] = e;
    expected_output[i] = e.Inverse();
  }

  BatchInverse<FieldElementT>(input, output);
  EXPECT_EQ(expected_output, output);
}

/*
  Verifying nothing bad happens when the batch is empty.
*/
TYPED_TEST(FieldAxiomTest, BatchInverseEmptySet) {
  using FieldElementT = TypeParam;
  std::vector<FieldElementT> input;
  std::vector<FieldElementT> output;

  BatchInverse<FieldElementT>(input, output);
}

TYPED_TEST(FieldAxiomTest, BatchInverseOneElement) {
  using FieldElementT = TypeParam;
  const FieldElementT val = RandomNonZeroElement<FieldElementT>(&this->prng);
  const std::vector<FieldElementT> input({val});
  const std::vector<FieldElementT> expected_output({val.Inverse()});
  std::vector<FieldElementT> output({FieldElementT::Zero()});

  BatchInverse<FieldElementT>(input, output);
  EXPECT_EQ(expected_output, output);
}

TYPED_TEST(FieldAxiomTest, BatchInverseTwoElement) {
  using FieldElementT = TypeParam;
  const FieldElementT val1 = RandomNonZeroElement<FieldElementT>(&this->prng);
  const FieldElementT val2 = RandomNonZeroElement<FieldElementT>(&this->prng);
  const std::vector<FieldElementT> input({val1, val2});
  const std::vector<FieldElementT> expected_output({val1.Inverse(), val2.Inverse()});
  std::vector<FieldElementT> output(2, FieldElementT::Zero());

  BatchInverse<FieldElementT>(input, output);
  EXPECT_EQ(expected_output, output);
}

TYPED_TEST(FieldAxiomTest, BatchInverseAsserts) {
  using FieldElementT = TypeParam;
  const size_t size = this->prng.UniformInt(1, 10);
  std::vector<FieldElementT> input(size, FieldElementT::Zero());
  std::vector<FieldElementT> output(size, FieldElementT::Zero());
  for (FieldElementT e : input) {
    e = RandomNonZeroElement<FieldElementT>(&this->prng);
  }

  // Set one element to be zero.
  input[this->prng.template UniformInt<size_t>(0, size - 1)] = FieldElementT::Zero();

  EXPECT_ASSERT(BatchInverse<FieldElementT>(input, output), testing::HasSubstr("zero"));
  EXPECT_ASSERT(
      BatchInverse<FieldElementT>(input, input), testing::HasSubstr("in place is not supported"));
  EXPECT_ASSERT(
      BatchInverse<FieldElementT>(
          std::vector<FieldElementT>(output.size() + 1, FieldElementT::One()), output),
      testing::HasSubstr("Size mismatch"));
}

// ----------------- BatchInverse for span of spans --------------------

TYPED_TEST(FieldAxiomTest, BatchInverseGeneralSpansMatrix) {
  using FieldElementT = TypeParam;
  // Initialize input, ouput and expected output vectors.
  const size_t n_cols = this->prng.UniformInt(0, 10);
  std::vector<std::vector<FieldElementT>> input(n_cols);
  std::vector<std::vector<FieldElementT>> expected_output(n_cols);
  std::vector<std::vector<FieldElementT>> output(n_cols);
  for (size_t i = 0; i < n_cols; ++i) {
    const size_t n_rows = this->prng.UniformInt(0, 10);
    for (size_t j = 0; j < n_rows; j++) {
      const FieldElementT input_val = RandomNonZeroElement<FieldElementT>(&this->prng);
      input[i].push_back(input_val);
      expected_output[i].push_back(input_val.Inverse());
      output[i].push_back(FieldElementT::Zero());
    }
  }
  BatchInverse<FieldElementT>(
      std::vector<gsl::span<const FieldElementT>>(input.begin(), input.end()),
      std::vector<gsl::span<FieldElementT>>(output.begin(), output.end()));
  EXPECT_EQ(expected_output, output);
}

/*
  Verifies nothing bad happens when the batch is empty.
*/
TYPED_TEST(FieldAxiomTest, BatchInverseEmptyMatrix) {
  using FieldElementT = TypeParam;
  BatchInverse<FieldElementT>(
      std::vector<gsl::span<const FieldElementT>>{}, std::vector<gsl::span<FieldElementT>>{});
}

/*
  Verifies nothing bad happens when the input contains only empty spans.
*/
TYPED_TEST(FieldAxiomTest, BatchInverseSpanOfEmptySpans) {
  using FieldElementT = TypeParam;

  const size_t n_cols = this->prng.UniformInt(0, 10);
  std::vector<std::vector<FieldElementT>> input(n_cols);
  std::vector<std::vector<FieldElementT>> expected_output(n_cols);
  std::vector<std::vector<FieldElementT>> output(n_cols);

  BatchInverse<FieldElementT>(
      std::vector<gsl::span<const FieldElementT>>(input.begin(), input.end()),
      std::vector<gsl::span<FieldElementT>>(output.begin(), output.end()));
  EXPECT_EQ(expected_output, output);
}

TYPED_TEST(FieldAxiomTest, BatchInverseMatrixAsserts) {
  using FieldElementT = TypeParam;
  const size_t n_cols = this->prng.UniformInt(1, 10);
  const size_t n_rows = this->prng.UniformInt(1, 10);
  std::vector<std::vector<FieldElementT>> input(
      n_cols, std::vector<FieldElementT>(n_rows, FieldElementT::Zero()));
  std::vector<std::vector<FieldElementT>> output(
      n_cols, std::vector<FieldElementT>(n_rows, FieldElementT::Zero()));
  for (size_t col = 0; col < input.size(); col++) {
    for (FieldElementT& e : input[col]) {
      e = RandomNonZeroElement<FieldElementT>(&this->prng);
    }
  }

  // Set one element to be zero.
  input[this->prng.template UniformInt<size_t>(0, n_cols - 1)]
       [this->prng.template UniformInt<size_t>(0, n_rows - 1)] = FieldElementT::Zero();

  const std::vector<gsl::span<const FieldElementT>> input_spans(input.begin(), input.end());

  EXPECT_ASSERT(
      BatchInverse<FieldElementT>(
          input_spans, std::vector<gsl::span<FieldElementT>>(output.begin(), output.end())),
      testing::HasSubstr("zero"));

  EXPECT_ASSERT(
      BatchInverse<FieldElementT>(
          input_spans, std::vector<gsl::span<FieldElementT>>(input.begin(), input.end())),
      testing::HasSubstr("in place is not supported"));

  // Mismatch of sizes.
  output.push_back({});
  EXPECT_ASSERT(
      BatchInverse<FieldElementT>(
          input_spans, std::vector<gsl::span<FieldElementT>>(output.begin(), output.end())),
      testing::HasSubstr("Size mismatch"));

  std::vector<std::vector<FieldElementT>> sec_output(
      n_cols, std::vector<FieldElementT>(n_rows, FieldElementT::Zero()));
  sec_output[this->prng.template UniformInt<size_t>(0, n_cols - 1)].push_back(
      FieldElementT::Zero());

  EXPECT_ASSERT(
      BatchInverse<FieldElementT>(
          input_spans, std::vector<gsl::span<FieldElementT>>(sec_output.begin(), sec_output.end())),
      testing::HasSubstr("Size mismatch"));
}

// --- Test generator and prime factors ---

/*
  Test that PrimeFactors() indeed returns all the prime factors of the field's size.
*/
TYPED_TEST(AllFieldsTest, Factors) {
  using IntType = decltype(TypeParam::FieldSize());
  auto factors = TypeParam::PrimeFactors();
  IntType cur = IntType::Sub(TypeParam::FieldSize(), IntType::One()).first;
  // We divide by the factors as much as we can and hopefully reach 1, this is to circumvent the
  // need to save the exponents of the prime factors.
  for (const IntType& factor : factors) {
    ASSERT_NE(factor, IntType::Zero());
    ASSERT_NE(factor, IntType::One());
    // Make sure the factor divides the group size at least once.
    IntType quotient, remainder;
    std::tie(quotient, remainder) = IntType::Div(cur, factor);
    ASSERT_EQ(remainder, IntType::Zero());
    // Keep dividing while the quotient is divisible by the factor.
    while (remainder == IntType::Zero()) {
      cur = quotient;
      std::tie(quotient, remainder) = IntType::Div(cur, factor);
    }
  }
  ASSERT_EQ(IntType(1), cur);
}

/*
  Test that a generator g of a field indeed generates its multiplicative group G by:
  1. Checking that g^|G| = 1.
  2. Checking that for every prime factor p of |G|, g^(|G|/p) != 1.
  Assuming that all prime factors are checked (which we test for in a separate test), this is
  equivalent to checking that g^1, g^2, g^3,...,g^|G| is in fact all of G.
*/
TYPED_TEST(AllFieldsTest, Generator) {
  using IntType = decltype(TypeParam::FieldSize());
  auto factors = TypeParam::PrimeFactors();
  IntType group_size = IntType::Sub(TypeParam::FieldSize(), IntType::One()).first;
  TypeParam raised_to_max_power = Pow(TypeParam::Generator(), group_size.ToBoolVector());
  ASSERT_EQ(raised_to_max_power, TypeParam::One());
  for (IntType factor : factors) {
    LOG(INFO) << "Checking for factor " << factor;
    IntType quotient, remainder;
    std::tie(quotient, remainder) = IntType::Div(group_size, factor);
    ASSERT_EQ(remainder, IntType::Zero());
    TypeParam raised = Pow(TypeParam::Generator(), quotient.ToBoolVector());
    ASSERT_NE(raised, TypeParam::One());
  }
}

TYPED_TEST(FieldAxiomTest, UninitializedFieldElementArray) {
  using FieldElementT = TypeParam;
  auto res = UninitializedFieldElementArray<FieldElementT, 4>();
  static_assert(
      std::is_same_v<decltype(res), std::array<FieldElementT, 4>>,
      "UninitializedFieldElementArray returned an unexpected type.");
}

}  // namespace
}  // namespace starkware
