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

#include "starkware/algebra/big_int.h"

#include <limits>

#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using TestTypes = ::testing::Types<BigInt<1>, BigInt<2>, BigInt<4>, BigInt<5>, BigInt<10>>;

template <typename>
class BigIntTest : public ::testing::Test {
 public:
  Prng prng;
};

TYPED_TEST_CASE(BigIntTest, TestTypes);

TYPED_TEST(BigIntTest, ToNibbleArray1) {
  using BigIntT = TypeParam;
  Prng prng;
  BigIntT bigint_sel = BigIntT::RandomBigInt(&prng);
  const auto nibbles = bigint_sel.ToNibbleArray();

  // Breaking the number apart and comparing nibbles.
  for (const uint8_t nibble : nibbles) {
    EXPECT_EQ(nibble, bigint_sel[0] % 16);
    bigint_sel >>= 4;
  }
  // The entire number should have been shifted at this point, all bits should equal 0.
  EXPECT_EQ(bigint_sel, BigIntT::Zero());
}

TYPED_TEST(BigIntTest, ToNibbleArray2) {
  using BigIntT = TypeParam;
  Prng prng;
  const BigIntT reference_bigint = BigIntT::RandomBigInt(&prng);
  auto nibbles = reference_bigint.ToNibbleArray();
  std::reverse(std::begin(nibbles), std::end(nibbles));  // little-endian to big-endian.

  // Constructing the BigInt from nibbles.
  BigIntT test_bigint = BigIntT::Zero();
  for (const uint8_t nibble : nibbles) {
    test_bigint <<= 4;
    test_bigint += BigInt<1>(nibble);
  }
  EXPECT_EQ(reference_bigint, test_bigint);
}

TEST(BigInt, Div) {
  BigInt<2> a({0, 1});
  BigInt<2> b({5, 0});

  EXPECT_EQ(
      BigInt<2>::Div(a, b), std::make_pair(BigInt<2>({0x3333333333333333L, 0}), BigInt<2>({1, 0})));
}

TEST(BigInt, DivByZero) {
  BigInt<2> a({0, 1});

  EXPECT_ASSERT(BigInt<2>::Div(a, BigInt<2>(0)), testing::HasSubstr("must not be zero"));
}

TEST(BigInt, DivRandom) {
  Prng prng;
  const BigInt<2> a = BigInt<2>::RandomBigInt(&prng);
  const BigInt<2> b = BigInt<2>::RandomBigInt(&prng);
  const auto& [q, r] = BigInt<2>::Div(a, b);
  EXPECT_EQ(BigInt<4>(a), q * b + r);
  EXPECT_LT(r, b);
}

TEST(BigInt, DivNoRemainder) {
  BigInt<2> a({20, 15});
  BigInt<2> b({5, 0});

  EXPECT_EQ(BigInt<2>::Div(a, b), std::make_pair(BigInt<2>({4, 3}), BigInt<2>({0, 0})));
}

TEST(BigInt, Inv) {
  BigInt<2> p({0xd80617e084679625, 0x7e5032470e0a7f8e});
  BigInt<2> a({18, 357});

  BigInt<2> expected_res({0x5c3d33fe0b586f40, 0x6741e17ed2831cc2});
  EXPECT_EQ(a.Inverse(a, p), expected_res);
}

TEST(BigInt, Random) {
  Prng prng;
  for (size_t i = 0; i < 100; ++i) {
    BigInt<2> a = BigInt<2>::RandomBigInt(&prng);
    BigInt<2> b = BigInt<2>::RandomBigInt(&prng);
    EXPECT_NE(a, b);
  }
}

TEST(BigInt, Log2Floor) {
  BigInt<5> a = BigInt<5>::One();
  bool carry;
  // Test powers of two.
  for (size_t i = 0; i < 64 * 5; ++i) {
    EXPECT_EQ(a.Log2Floor(), i);
    std::tie(a, carry) = BigInt<5>::Add(a, a);
  }
  BigInt<5> b = BigInt<5>::One();
  // Test powers of two plus one.
  for (size_t i = 0; i < 64 * 5; ++i) {
    EXPECT_EQ(b.Log2Floor(), i);
    std::tie(b, carry) = BigInt<5>::Add(b, b);
    std::tie(b, carry) = BigInt<5>::Add(b, BigInt<5>::One());
  }
  static_assert(BigInt<5>(7).Log2Floor() == 2, "Should work.");
}

TEST(BigInt, NumLeadingZeros) {
  constexpr BigInt<5> kOne = BigInt<5>::One();

  static_assert(
      BigInt<5>::kDigits - 1 == kOne.NumLeadingZeros(),
      "All digit except the last one should be zero");
  static_assert(0 == (-kOne).NumLeadingZeros(), "Should have 0 leading zero's");

  static_assert(BigInt<5>::kDigits == BigInt<5>::Zero().NumLeadingZeros(), "All the digits are 0");

  EXPECT_EQ(BigInt<5>({17, 0, 0, 0, 0}).NumLeadingZeros(), BigInt<5>::kDigits - 5U);
  EXPECT_EQ(BigInt<5>({0, 4, 0, 0, 0}).NumLeadingZeros(), BigInt<5>::kDigits - 67U);
  EXPECT_EQ(BigInt<5>({0, 1, 0, 0, 17}).NumLeadingZeros(), 59U);
  EXPECT_EQ(BigInt<5>({0, 1, 0, 0, 1}).NumLeadingZeros(), 63U);
  static_assert(BigInt<5>(7).NumLeadingZeros() == BigInt<5>::kDigits - 3U, "Should work.");
}

TEST(BigInt, Log2Ceil) {
  EXPECT_EQ(BigInt<5>({17, 0, 0, 0, 0}).Log2Ceil(), 5U);
  EXPECT_EQ(BigInt<5>({0, 4, 0, 0, 0}).Log2Ceil(), 66U);
  EXPECT_EQ(BigInt<5>({0, 1, 0, 0, 17}).Log2Ceil(), 261U);
  EXPECT_EQ(BigInt<5>({0, 1, 0, 0, 1}).Log2Ceil(), 257U);
  static_assert(BigInt<5>(7).Log2Ceil() == 3U, "Should work.");
}

TEST(BigInt, IsPowerOfTwo) {
  EXPECT_EQ(BigInt<5>({17, 0, 0, 0, 0}).IsPowerOfTwo(), false);
  EXPECT_EQ(BigInt<5>({8, 0, 0, 0, 0}).IsPowerOfTwo(), true);
  EXPECT_EQ(BigInt<5>({8, 0, 1, 0, 0}).IsPowerOfTwo(), false);
  EXPECT_EQ(BigInt<5>({0, 0, 0, 16, 0}).IsPowerOfTwo(), true);
  static_assert(!BigInt<5>(7).IsPowerOfTwo(), "Should work.");
  static_assert(BigInt<5>(8).IsPowerOfTwo(), "Should work.");
}

template <size_t N>
void BitwiseOpTest(Prng* prng) {
  using IntT = BigInt<N>;
  const IntT a = IntT::RandomBigInt(prng);
  const IntT b = IntT::RandomBigInt(prng);

  EXPECT_EQ(a & a, a);
  EXPECT_EQ(a & IntT::Zero(), IntT::Zero());

  EXPECT_EQ(a ^ a, IntT::Zero());
  EXPECT_EQ(a ^ IntT::Zero(), a);

  EXPECT_EQ(a | a, a);
  EXPECT_EQ(a | IntT::Zero(), a);

  const IntT c = a & b;
  const IntT d = a ^ b;
  const IntT e = a | b;

  EXPECT_EQ(c + d, e);
  EXPECT_EQ(c | d, e);
  EXPECT_EQ(c ^ d, e);
  EXPECT_EQ(c & d, IntT::Zero());

  for (size_t i = 0; i < N; ++i) {
    EXPECT_EQ(a[i] & b[i], c[i]) << "N = " << N;
    EXPECT_EQ(a[i] ^ b[i], d[i]) << "N = " << N;
    EXPECT_EQ(a[i] | b[i], e[i]) << "N = " << N;
  }
}

TEST(BigInt, BitwiseOp) {
  Prng prng;
  BitwiseOpTest<1>(&prng);
  BitwiseOpTest<2>(&prng);
  BitwiseOpTest<5>(&prng);
  BitwiseOpTest<10>(&prng);
}

TEST(BigInt, Multiplication) {
  constexpr size_t kN1 = 1;
  constexpr size_t kN2 = 2;
  constexpr size_t kN3 = 10;

  static_assert(
      BigInt<kN1>::Zero() * BigInt<kN1>::Zero() == BigInt<2 * kN1>::Zero(), "0*0 is not 0");
  static_assert(
      BigInt<kN3>::Zero() * BigInt<kN3>::Zero() == BigInt<2 * kN3>::Zero(), "0*0 is not 0");
  static_assert(BigInt<kN2>::One() * BigInt<kN2>::One() == BigInt<2 * kN2>::One(), "1*1 is not 1");
  static_assert(
      BigInt<kN1>(Pow2(23)) * BigInt<kN1>(Pow2(27)) == BigInt<2 * kN1>(Pow2(50)),
      "(2^23)*(2^27) is not 2^50");
  static_assert(BigInt<kN2>({0, 17}) * BigInt<kN2>({0, 15}) == BigInt<2 * kN2>({0, 0, 255, 0}));
  static_assert(
      0x45467f1b1b72b92a_Z * 0x5a24f03a01d5b10c_Z == 0x1864c79b3117812a6d564ff0d558b7f8_Z);
  static_assert(
      0x5342b50c88dbce0db6fe1c672256eb8d_Z * 0xf42ff50167e9c6cca4d5b18636b1516e_Z ==
      0x4f6b2d7e7c1233fdc642edeefc766bc635729fa19af730c8cf66b2c4dc5dd396_Z);
  static_assert(
      0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_Z *
          0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff_Z ==
      BigInt<8>({0x1, 0x0, 0x0, 0x0, 0xfffffffffffffffe, 0xffffffffffffffff, 0xffffffffffffffff,
                 0xffffffffffffffff}));
}

TEST(BigInt, AddMod) {
  const BigInt<4> minus_one = BigInt<4>::Zero() - BigInt<4>::One();
  const BigInt<4> m = BigInt<4>(8);
  const BigInt<4> res = BigInt<4>::AddMod(minus_one, minus_one, m);
  EXPECT_EQ(res, BigInt<4>(6));
}

TEST(BigInt, MulMod) {
  const BigInt<4> minus_one = BigInt<4>::Zero() - BigInt<4>::One();
  const BigInt<4> m = BigInt<4>(8);
  const BigInt<4> res = BigInt<4>::MulMod(minus_one, minus_one, m);
  EXPECT_EQ(res, BigInt<4>(1));

  EXPECT_EQ(BigInt<4>::MulMod(BigInt<4>(7), BigInt<4>(5), BigInt<4>(32)), BigInt<4>(3));
}

template <size_t N>
void BinaryShiftTest(Prng* prng) {
  using IntT = BigInt<N>;
  const size_t n_bits_word = 8 * sizeof(uint64_t);

  static_assert((BigInt<1>(1) <<= 1) == BigInt<1>(2), "Check that operator<<= is constexpr.");

  {
    // Shift right by 1 is equivalent to division by 2.
    const IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    b >>= 1;
    EXPECT_EQ(b, IntT::Div(a, IntT(2)).first);
  }

  {
    // Shift left by 1 is equivalent to multiplication by 2.
    const IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    b <<= 1;
    EXPECT_EQ(b, a + a);
  }

  {
    // Big shift right clears all bits.
    IntT a = IntT::RandomBigInt(prng);
    a >>= N * n_bits_word;
    EXPECT_EQ(a, IntT::Zero());
  }

  {
    // Big shift left clears all bits.
    IntT a = IntT::RandomBigInt(prng);
    a <<= N * n_bits_word;
    EXPECT_EQ(a, IntT::Zero());
  }

  {
    // Big shift right clears all bits.
    IntT a = IntT::RandomBigInt(prng);
    a >>= N * n_bits_word + prng->UniformInt(0, 100);
    EXPECT_EQ(a, IntT::Zero());
  }

  {
    // Big shift left clears all bits.
    IntT a = IntT::RandomBigInt(prng);
    a <<= N * n_bits_word + prng->UniformInt(0, 100);
    EXPECT_EQ(a, IntT::Zero());
  }

  {
    // No shift right - no change.
    const IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    b >>= N * 0;
    EXPECT_EQ(a, b);
  }

  {
    // No shift left - no change.
    const IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    b <<= N * 0;
    EXPECT_EQ(a, b);
  }

  {
    // Test shift right of entire words.
    const IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    const auto n_words_shift = prng->template UniformInt<size_t>(0, N);
    b >>= (n_words_shift * n_bits_word);
    for (size_t i = 0; i < N; ++i) {
      EXPECT_EQ(b[i], (n_words_shift + i) < N ? a[n_words_shift + i] : 0)
          << "N = " << N << "; i = " << i << " n_words_shift = " << n_words_shift;
    }
  }

  {
    // Test shift left of entire words.
    const IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    const auto n_words_shift = prng->template UniformInt<size_t>(0, N);
    b <<= (n_words_shift * n_bits_word);
    for (size_t i = 0; i < N; ++i) {
      EXPECT_EQ(b[i], i >= n_words_shift ? a[i - n_words_shift] : 0)
          << "N = " << N << "; i = " << i << " n_words_shift = " << n_words_shift;
    }
  }

  {
    // Test shift right with number of bits less then a word.
    const IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    const auto n_bits_shift = prng->template UniformInt<size_t>(1, n_bits_word - 1);
    b >>= n_bits_shift;
    for (size_t i = 0; i < N; ++i) {
      const uint64_t carry = (i < N - 1 ? (a[i + 1] << (n_bits_word - n_bits_shift)) : 0);
      EXPECT_EQ(b[i], (a[i] >> n_bits_shift) ^ carry)
          << "N = " << N << "; i = " << i << " n_bits_shift = " << n_bits_shift;
    }
  }

  {
    // Test shift left with number of bits less then a word.
    const IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    const auto n_bits_shift = prng->template UniformInt<size_t>(1, n_bits_word - 1);
    b <<= n_bits_shift;
    for (size_t i = 0; i < N; ++i) {
      const uint64_t carry = (i > 0 ? (a[i - 1] >> (n_bits_word - n_bits_shift)) : 0);
      EXPECT_EQ(b[i], (a[i] << n_bits_shift) ^ carry)
          << "N = " << N << "; i = " << i << " n_bits_shift = " << n_bits_shift;
    }
  }

  {
    // Arbitrary shift right, is equivalent to separately shifting entire words and bits internally.
    IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    IntT c = a;
    const auto n_bits_shift = prng->template UniformInt<size_t>(0, n_bits_word - 1);
    const auto n_words_shift = prng->template UniformInt<size_t>(0, N - 1);

    a >>= n_bits_shift + n_words_shift * n_bits_word;

    b >>= n_bits_shift;
    b >>= n_words_shift * n_bits_word;

    c >>= n_words_shift * n_bits_word;
    c >>= n_bits_shift;

    EXPECT_EQ(a, b);
    EXPECT_EQ(a, c);
  }

  {
    // Arbitrary shift left, is equivalent to separately shifting entire words and bits internally.
    IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    IntT c = a;
    const auto n_bits_shift = prng->template UniformInt<size_t>(0, n_bits_word - 1);
    const auto n_words_shift = prng->template UniformInt<size_t>(0, N - 1);

    a <<= n_bits_shift + n_words_shift * n_bits_word;

    b <<= n_bits_shift;
    b <<= n_words_shift * n_bits_word;

    c <<= n_words_shift * n_bits_word;
    c <<= n_bits_shift;

    EXPECT_EQ(a, b);
    EXPECT_EQ(a, c);
  }

  {
    // Arbitrary shift right and left.
    IntT a = IntT::RandomBigInt(prng);
    IntT b = a;
    const IntT c = a;
    const auto n_bits_shift = prng->template UniformInt<size_t>(0, N * n_bits_word - 1);

    a <<= n_bits_shift;
    a >>= n_bits_shift;

    b >>= N * n_bits_word - n_bits_shift;
    b <<= N * n_bits_word - n_bits_shift;

    // The shifts in a deleted n_bits_shift left bits. The shifts in b deleted all other bits. so
    // their sum should be the original number.
    EXPECT_EQ(a + b, c);
  }
}

TEST(BigInt, BinaryShift) {
  Prng prng;
  BinaryShiftTest<1>(&prng);
  BinaryShiftTest<2>(&prng);
  BinaryShiftTest<5>(&prng);
  BinaryShiftTest<10>(&prng);
}

TEST(BigInt, ToFromString) {
  Prng prng;
  for (size_t i = 0; i < 17; ++i) {
    BigInt<17> b0 = BigInt<17>::RandomBigInt(&prng);
    std::stringstream s;
    s << b0;
    ASSERT_EQ(b0, BigInt<17>::FromString(s.str()));
  }
}

TEST(BigInt, IsEven) {
  Prng prng;
  ASSERT_TRUE(BigInt<2>({0, 0}).IsEven());
  ASSERT_FALSE(BigInt<2>({3, 0}).IsEven());
  ASSERT_TRUE(BigInt<2>({6, 5}).IsEven());
  ASSERT_FALSE(BigInt<2>({3, 1}).IsEven());

  const auto x = prng.UniformInt<uint64_t>(0, 1000);
  ASSERT_EQ(x % 2 == 0, BigInt<2>({x, prng.UniformInt<uint64_t>(0, 1000)}).IsEven());
}

TEST(BigInt, IsMsbSet) {
  Prng prng;
  constexpr uint64_t kMsbMask = Pow2(std::numeric_limits<uint64_t>::digits - 1);
  ASSERT_TRUE(BigInt<2>({0, ~UINT64_C(0)}).IsMsbSet());
  ASSERT_FALSE(BigInt<2>({3, 0}).IsMsbSet());
  ASSERT_TRUE(BigInt<2>({6, kMsbMask}).IsMsbSet());
  ASSERT_FALSE(BigInt<2>({3, 1}).IsMsbSet());

  const auto x = prng.UniformInt<uint64_t>(0, 1000);
  ASSERT_EQ((x & kMsbMask) != 0, BigInt<2>({prng.UniformInt<uint64_t>(0, 1000), x}).IsMsbSet());
}

TEST(BigInt, UserLiteral) {
  auto a = 0x73eda753299d7d483339d80809a1d80553bda402fffe5bfeffffffff00000001_Z;
  BigInt<4> b({0xffffffff00000001, 0x53bda402fffe5bfe, 0x3339d80809a1d805, 0x73eda753299d7d48});

  static_assert(std::is_same_v<decltype(a), decltype(b)>, "decltype(a) should be BigInt<4>");

  ASSERT_EQ(a, b);
}

TEST(BigInt, UserLiteral2) {
  auto zero = 0x0_Z;
  BigInt<1> one_limb_zero(0);
  BigInt<2> two_limb_zero(0);

  static_assert(
      std::is_same_v<decltype(zero), decltype(one_limb_zero)>,
      "decltype(zero) should be BigInt<1>");
  static_assert(
      !std::is_same_v<decltype(zero), decltype(two_limb_zero)>,
      "decltype(zero) shouldn't be BigInt<2>");

  ASSERT_EQ(one_limb_zero, zero);
  ASSERT_EQ(two_limb_zero, zero);  // Works due to implicit cast of BigInt<2> to BigInt<1>.
}

constexpr BigInt<1> BigWithVal(uint64_t val) {
  BigInt<1> res{0};
  res[0] = val;
  return res;
}

TEST(BigInt, ConstexprTest) {
  constexpr auto kVal = 0x18_Z;

  static_assert(kVal.IsEven(), "IsEven() should return true");

  static_assert(kVal == 0x18_Z, "should be equal");
  static_assert(kVal != 0x27_Z, "shouldn't be equal");

  static_assert(kVal[0] == 0x18, "should be equal");
  static_assert(std::is_same_v<decltype(kVal[0]), const uint64_t&>, "bad type");

  static_assert(BigWithVal(13)[0] == 13, "should be equal");

  static_assert(Umul128(13, 4) == Uint128(52), "should be equal");
  static_assert(BigInt<2>(46) < BigInt<2>(87), "should work");
  static_assert(BigInt<2>(146) >= BigInt<2>(87), "should work");
  static_assert(BigInt<2>::Inverse(BigInt<2>(5), BigInt<2>(3)) == BigInt<2>(2), "should work");
}

TEST(BigInt, BigIntWidening) {
  ASSERT_EQ(BigInt<2>({0xffffffff00000001, 0}), 0xffffffff00000001_Z);
  ASSERT_EQ(BigInt<3>({0xffffffff00000001, 0x17, 0}), BigInt<2>({0xffffffff00000001, 0x17}));
}

TEST(BigInt, ConstexprBigIntWidening) {
  static_assert(BigInt<2>(0x1_Z) != BigInt<2>(0x2_Z), "should not be equal");
}

/*
  Check that val * 2^-64 == val *x^192 * 2^-256 (mod modulus).
*/
TEST(BigInt, OneLimbMontgomeryReduction) {
  Prng prng;
  auto modulus = 0x73eda753299d7d483339d80809a1d80553bda402fffe5bfeffffffff00000001_Z;
  auto mprime = 18446744069414584319UL;
  auto val = BigInt<4>::RandomBigInt(&prng);
  auto res = BigInt<4>::OneLimbMontgomeryReduction(val, modulus, mprime);
  auto mul_res = BigInt<4>::MontMul(val, BigInt<4>({0, 0, 0, 1}), modulus, mprime);
  auto res2 = BigInt<4>::ReduceIfNeeded(mul_res, modulus);
  EXPECT_EQ(res, res2);
}

TEST(BigInt, Serialization) {
  auto num = 0x76d8a6ce180b83a1c1b9cdd9b505e1cce9959ce7c0f4e084b189091985121ece_Z;
  std::array<std::byte, 32> data{};
  num.ToBytes(data, true);
  EXPECT_EQ(
      BytesToHexString(data), "0x76d8a6ce180b83a1c1b9cdd9b505e1cce9959ce7c0f4e084b189091985121ece");
  num.ToBytes(data, false);
  EXPECT_EQ(
      BytesToHexString(data), "0xce1e1285190989b184e0f4c0e79c95e9cce105b5d9cdb9c1a1830b18cea6d876");

  EXPECT_EQ(
      BigInt<4>::FromBytes(data, true),
      0xce1e1285190989b184e0f4c0e79c95e9cce105b5d9cdb9c1a1830b18cea6d876_Z);
  EXPECT_EQ(
      BigInt<4>::FromBytes(data, false),
      0x76d8a6ce180b83a1c1b9cdd9b505e1cce9959ce7c0f4e084b189091985121ece_Z);
}

}  // namespace
}  // namespace starkware
