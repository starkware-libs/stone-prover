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

using testing::Contains;
using testing::ElementsAreArray;
using testing::HasSubstr;

template <typename T>
class FieldTest : public ::testing::Test {
 public:
  // This allows each test to use a field f of type TypeParam, with a PRNG called prng.
  Field f;
  Prng prng;
  FieldTest() : f(Field::Create<T>()) {}
  // A convenience function to avoid the mess of "this->f.RandomElement(&this->prng)" every time.
  auto RandomElement() { return f.RandomElement(&prng); }
};

using TestedFieldTypes = ::testing::Types<
    TestFieldElement, LongFieldElement, PrimeFieldElement<252, 0>,
    FractionFieldElement<TestFieldElement>, FractionFieldElement<PrimeFieldElement<252, 0>>,
    ExtensionFieldElement<TestFieldElement>, ExtensionFieldElement<PrimeFieldElement<252, 0>>>;
TYPED_TEST_CASE(FieldTest, TestedFieldTypes);

template <typename T>
class PrimeFieldsTest : public ::testing::Test {};

using PrimeFieldTypes = ::testing::Types<
    TestFieldElement, LongFieldElement, PrimeFieldElement<252, 0>,
    FractionFieldElement<TestFieldElement>, FractionFieldElement<PrimeFieldElement<252, 0>>,
    ExtensionFieldElement<TestFieldElement>, ExtensionFieldElement<PrimeFieldElement<252, 0>>>;
TYPED_TEST_CASE(PrimeFieldsTest, PrimeFieldTypes);

// --- Test operators ---

TYPED_TEST(FieldTest, EqOperator) {
  for (unsigned i = 0; i < 100; ++i) {
    auto r = this->RandomElement();
    ASSERT_TRUE(r == r);
  }
}

TYPED_TEST(FieldTest, NotEqOperator) {
  for (unsigned i = 0; i < 100; ++i) {
    auto r = this->RandomElement();
    auto r_plus_one = r + this->f.One();
    ASSERT_TRUE(r != r_plus_one);
    ASSERT_TRUE(r_plus_one != r);
  }
}

TYPED_TEST(FieldTest, Assignment) {
  for (unsigned i = 0; i < 100; ++i) {
    auto r = this->RandomElement();
    auto x = r;  // NOLINT
    ASSERT_TRUE(r == x);
  }
}

TYPED_TEST(FieldTest, DivisionIsMultiplicationByInverse) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomElement();
    auto b = this->RandomElement();
    if (b != this->f.Zero()) {
      auto b_inverse = b.Inverse();
      ASSERT_EQ(a * b_inverse, a / b);
    }
  }
}

TYPED_TEST(FieldTest, UnaryMinusIsLikeZeroMinusElement) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomElement();
    ASSERT_EQ(this->f.Zero() - a, -a);
  }
}

/*
  Checks that (-a) + (b) = b - a.
*/
TYPED_TEST(FieldTest, UnaryMinusIsLikeMinusOperation) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomElement();
    auto b = this->RandomElement();
    ASSERT_EQ(b - a, -a + b);
  }
}

TYPED_TEST(FieldTest, UnaryMinusZeroIsZero) {
  auto zero = this->f.Zero();
  EXPECT_EQ(-zero, zero);
}

TYPED_TEST(FieldTest, MinusMinusIsPlus) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomElement();
    auto b = this->RandomElement();
    ASSERT_EQ(a + b, a - (-b));
  }
}

TYPED_TEST(FieldTest, MultiplyByZero) {
  EXPECT_EQ(this->f.Zero(), this->RandomElement() * this->f.Zero());
}

TYPED_TEST(FieldTest, InverseZero) { EXPECT_ASSERT(this->f.Zero().Inverse(), testing::_); }

TYPED_TEST(FieldTest, DivisionByZero) {
  const auto rand = this->RandomElement();
  EXPECT_ASSERT(rand / this->f.Zero(), testing::_);
  EXPECT_ASSERT(this->f.Zero() / this->f.Zero(), testing::_);
}

TYPED_TEST(FieldTest, InverseOne) { EXPECT_EQ(this->f.One().Inverse(), this->f.One()); }

TYPED_TEST(FieldTest, DivisionByOne) {
  const auto rand = this->RandomElement();
  EXPECT_EQ(rand / this->f.One(), rand);
  EXPECT_EQ(this->f.One() / this->f.One(), this->f.One());
}

// --- Test other operations ---

TYPED_TEST(FieldTest, Times) {
  auto a = TypeParam::RandomElement(&this->prng);
  auto sum = TypeParam::Zero();

  for (uint8_t i = 0; i < 10; ++i) {
    ASSERT_EQ(sum, Times(i, a));
    sum += a;
  }
}

TYPED_TEST(FieldTest, Pow) {
  auto a = TypeParam::RandomElement(&this->prng);
  auto power = TypeParam::One();

  for (int i = 0; i < 10; ++i) {
    ASSERT_EQ(power, Pow(a, i));
    power *= a;
  }
}

TYPED_TEST(FieldTest, BatchPowEmpty) {
  using FieldElementT = TypeParam;
  const FieldElementT base = FieldElementT::RandomElement(&this->prng);

  // Verify does not crash for empty input.
  std::vector<FieldElementT> res;
  BatchPow<FieldElementT>(base, {}, res);
  EXPECT_TRUE(res.empty());
}

TYPED_TEST(FieldTest, BatchPowSingle) {
  using FieldElementT = TypeParam;
  const FieldElementT base = FieldElementT::RandomElement(&this->prng);

  // Verify the case of single element.
  uint64_t exp = this->prng.UniformInt(0, 1000);
  std::vector<FieldElementT> res({FieldElementT::Zero()});
  BatchPow<FieldElementT>(base, {exp}, res);
  EXPECT_EQ(Pow(base, exp), res[0]) << "base = " << base << "; exp = " << exp;
}

TYPED_TEST(FieldTest, BatchPowSinglePow2) {
  using FieldElementT = TypeParam;
  const FieldElementT base = FieldElementT::RandomElement(&this->prng);

  // Verify the case of single element with exponent having a single bit on.
  uint64_t exp = Pow2(this->prng.UniformInt(0, 10));
  std::vector<FieldElementT> res({FieldElementT::Zero()});
  BatchPow<FieldElementT>(base, {exp}, res);
  EXPECT_EQ(Pow(base, exp), res[0]) << "base = " << base << "; exp = " << exp;
}

TYPED_TEST(FieldTest, BatchPowZeroExponents) {
  using FieldElementT = TypeParam;
  const FieldElementT base = FieldElementT::RandomElement(&this->prng);

  // Zero exponent.
  const size_t size = this->prng.UniformInt(1, 10);
  const std::vector<uint64_t> exp(size, 0);
  std::vector<FieldElementT> res(size, FieldElementT::Zero());
  BatchPow<FieldElementT>(base, exp, res);
  EXPECT_EQ(std::vector<FieldElementT>(size, FieldElementT::One()), res);
}

TYPED_TEST(FieldTest, BatchPowSomeZeroExponents) {
  using FieldElementT = TypeParam;
  const FieldElementT base = FieldElementT::RandomElement(&this->prng);

  // Zero with non-zero exponent.
  const size_t size = 4;
  const std::vector<uint64_t> exp({1, 2, 3, 4});
  std::vector<FieldElementT> res(size, FieldElementT::Zero());
  BatchPow<FieldElementT>(base, exp, res);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(Pow(base, exp[i]), res[i]);
  }
}

TYPED_TEST(FieldTest, BatchPowZeroBase) {
  using FieldElementT = TypeParam;
  const FieldElementT base = FieldElementT::Zero();

  // Test exponents of zeros are zeros.
  const size_t size = this->prng.UniformInt(0, 10);
  const std::vector<uint64_t> exp = this->prng.template UniformIntVector<uint64_t>(0, 10000, size);
  std::vector<FieldElementT> expected_output;
  expected_output.reserve(size);
  for (const uint64_t e : exp) {
    expected_output.push_back(e == 0 ? FieldElementT::One() : FieldElementT::Zero());
  }
  std::vector<FieldElementT> res(size, FieldElementT::RandomElement(&this->prng));
  BatchPow<FieldElementT>(base, exp, res);
  EXPECT_EQ(expected_output, res);
}

TYPED_TEST(FieldTest, BatchPowZeroBaseZeroExp) {
  using FieldElementT = TypeParam;
  const FieldElementT base = FieldElementT::Zero();

  // Test exponents of zeros are zeros.
  const size_t size = this->prng.UniformInt(0, 10);
  const std::vector<uint64_t> exp(size, 0);

  // 0^0=1.
  const std::vector<FieldElementT> expected_output(size, FieldElementT::One());

  std::vector<FieldElementT> res(size, FieldElementT::RandomElement(&this->prng));
  BatchPow<FieldElementT>(base, exp, res);
  EXPECT_EQ(expected_output, res);
}

TYPED_TEST(FieldTest, BatchPowRandom) {
  using FieldElementT = TypeParam;
  const FieldElementT base = FieldElementT::RandomElement(&this->prng);

  // Random test.
  const size_t size = this->prng.UniformInt(0, 10);
  const std::vector<uint64_t> exp = this->prng.template UniformIntVector<uint64_t>(0, 10000, size);
  std::vector<FieldElementT> res(size, FieldElementT::Zero());
  BatchPow<FieldElementT>(base, exp, res);
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(Pow(base, exp[i]), res[i]);
  }
}

TYPED_TEST(PrimeFieldsTest, IsSquareAndSqrt) {
  using FieldElementT = TypeParam;
  Prng prng;

  EXPECT_TRUE(IsSquare(FieldElementT::Zero()));
  EXPECT_EQ(FieldElementT::Zero(), Sqrt(FieldElementT::Zero()));
  EXPECT_TRUE(IsSquare(FieldElementT::One()));
  EXPECT_THAT(
      (std::array<FieldElementT, 2>{FieldElementT::One(), -FieldElementT::One()}),
      Contains(Sqrt(FieldElementT::One())));

  EXPECT_FALSE(IsSquare(FieldElementT::Generator()));

  FieldElementT x = FieldElementT::RandomElement(&prng);
  // Choose an element which is a square.
  x *= x;
  EXPECT_TRUE(IsSquare(x));
  FieldElementT sqrt = Sqrt(x);
  EXPECT_EQ(x, sqrt * sqrt);

  // Choose an element which is not a square.
  x *= FieldElementT::Generator();
  EXPECT_FALSE(IsSquare(x));
  EXPECT_ASSERT(Sqrt(x), HasSubstr("does not have a square root"));
}

TYPED_TEST(PrimeFieldsTest, FftButterFly) {
  using FieldElementT = TypeParam;
  Prng prng;
  auto a = FieldElementT::RandomElement(&prng);
  auto b = FieldElementT::RandomElement(&prng);
  auto twiddle_factor = FieldElementT::RandomElement(&prng);

  auto expected = UninitializedFieldElementArray<FieldElementT, 2>();
  auto actual = UninitializedFieldElementArray<FieldElementT, 2>();

  FieldElementBase<FieldElementT>::FftButterfly(a, b, twiddle_factor, &expected[0], &expected[1]);

  FieldElementT::FftButterfly(a, b, twiddle_factor, &actual[0], &actual[1]);
  FieldElementT::FftNormalize(&actual[0]);
  FieldElementT::FftNormalize(&actual[1]);
  EXPECT_THAT(expected, ElementsAreArray(actual));
}

}  // namespace
}  // namespace starkware
