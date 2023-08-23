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

#ifndef STARKWARE_ALGEBRA_FIELDS_BIG_PRIME_CONSTANTS_H_
#define STARKWARE_ALGEBRA_FIELDS_BIG_PRIME_CONSTANTS_H_

#include <array>

#include "starkware/algebra/big_int.h"

namespace starkware {

/*
  Contains a set of constants that go along with the prime number.
  NBits indicates the number of bits required to represent kModulus.
  Index indicates which large prime we're using as modulus.
  For example BigPrimeConstants<252, 0> and BigPrimeConstants<252, 3> are two different 252 bit
  primes.

  kModulus is the prime itself
  kMontgomeryR is the R used for the Montgomery multiplication, which is 2^256 mod kModulus
  kMontgomeryRSquared is R^2 mod kModulus
  kMontgomeryRCubed is R^3 mod kModulus
  kFactors are the prime factors of the multiplicative group's size.
  kMontgomeryMPrime := (-(kModulus^-1)) mod 2^64.
*/
template <int NBits, int Index>
struct BigPrimeConstants {
  // static_assert(false) does not depend on T, so we use Index == -1 which is always false.
  static_assert(Index == -1, "BigPrimeConstants is not implemented for the given index");
};

// The following BigPrimeConstants are field specific definitions. The index templated parameter
// indicates which large prime we're using as modulus.

template <>
struct BigPrimeConstants<252, 0> {
  using ValueType = BigInt<4>;

  static constexpr ValueType kModulus =
      0x800000000000011000000000000000000000000000000000000000000000001_Z;
  static constexpr ValueType kMontgomeryR =
      0x7fffffffffffdf0ffffffffffffffffffffffffffffffffffffffffffffffe1_Z;
  static constexpr ValueType kMontgomeryRSquared =
      0x7ffd4ab5e008810ffffffffff6f800000000001330ffffffffffd737e000401_Z;
  static constexpr ValueType kMontgomeryRCubed =
      0x38e5f79873c0a6df47d84f8363000187545706677ffcc06cc7177d1406df18e_Z;
  static constexpr std::array<ValueType, 5> kFactors{0x2_Z, 0x5_Z, 0x7_Z, 0x5e2430d_Z, 0x9f1e667_Z};
  static constexpr uint64_t kMontgomeryMPrime = ~uint64_t(0);
  static constexpr uint64_t kGenerator = 3;
  static constexpr ValueType kMaxDivisible =
      0xf80000000000020f00000000000000000000000000000000000000000000001f_Z;
};

template <>
struct BigPrimeConstants<254, 1> {
  using ValueType = BigInt<4>;

  static constexpr ValueType kModulus =
      0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47_Z;
  static constexpr ValueType kMontgomeryR =
      0x0e0a77c19a07df2f666ea36f7879462c0a78eb28f5c70b3dd35d438dc58f0d9d_Z;
  static constexpr ValueType kMontgomeryRSquared =
      0x06d89f71cab8351f47ab1eff0a417ff6b5e71911d44501fbf32cfc5b538afa89_Z;
  static constexpr ValueType kMontgomeryRCubed =
      0x20fd6e902d592544ef7f0b0c0ada0afb62f210e6a7283db6b1cd6dafda1530df_Z;
  static constexpr std::array<ValueType, 12> kFactors{
      0x2_Z,    0x3_Z,        0xd_Z,         0x1d_Z,
      0x43_Z,   0xe5_Z,       0x137_Z,       0x3d7_Z,
      0x2afb_Z, 0x1831fb5f_Z, 0x2ab6cbdc9_Z, 0x2775dec4d2fd445d02a32aa0f59b66aa11_Z};
  static constexpr uint64_t kMontgomeryMPrime = 9786893198990664585UL;
  static constexpr uint64_t kGenerator = 3;
  static constexpr ValueType kMaxDivisible =
      0xf1f5883e65f820d099915c908786b9d3f58714d70a38f4c22ca2bc723a70f263_Z;
};

template <>
struct BigPrimeConstants<254, 2> {
  using ValueType = BigInt<4>;

  static constexpr ValueType kModulus =
      0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001_Z;
  static constexpr ValueType kMontgomeryR =
      0x0e0a77c19a07df2f666ea36f7879462e36fc76959f60cd29ac96341c4ffffffb_Z;
  static constexpr ValueType kMontgomeryRSquared =
      0x0216d0b17f4e44a58c49833d53bb808553fe3ab1e35c59e31bb8e645ae216da7_Z;
  static constexpr ValueType kMontgomeryRCubed =
      0x0cf8594b7fcc657c893cc664a19fcfed2a489cbe1cfbb6b85e94d8e1b4bf0040_Z;
  static constexpr std::array<ValueType, 10> kFactors{0x2_Z,
                                                      0x3_Z,
                                                      0xd_Z,
                                                      0x1d_Z,
                                                      0x3d7_Z,
                                                      0x2afb_Z,
                                                      0x39e11_Z,
                                                      0x1831fb5f_Z,
                                                      0x5ef9dea338eb5_Z,
                                                      0x2ca6487cfcd795e8729527e1_Z};
  static constexpr uint64_t kMontgomeryMPrime = 14042775128853446655UL;
  static constexpr uint64_t kGenerator = 5;
  static constexpr ValueType kMaxDivisible =
      0xf1f5883e65f820d099915c908786b9d3f58714d70a38f4c22ca2bc723a70f263_Z;
};

template <>
struct BigPrimeConstants<252, 3> {
  using ValueType = BigInt<4>;

  static constexpr ValueType kModulus =
      0x800000000000000000000000000000000000000000040000000000000000001_Z;
  static constexpr ValueType kMontgomeryR =
      0x7fffffffffffffffffffffffffffffffffffffffff83fffffffffffffffffe1_Z;
  static constexpr ValueType kMontgomeryRSquared = 0x400000000000000000020000000000000000000400_Z;
  static constexpr ValueType kMontgomeryRCubed =
      0x5ffffffffffffffffffe7ffffffffffffffffffa0003fffffffffffffff8001_Z;
  static constexpr std::array<ValueType, 7> kFactors{
      0x2_Z, 0x3_Z, 0x15b_Z, 0x1039_Z, 0x83c7bb9d3_Z, 0xb1744941b_Z, 0x15c0460198604f187d739_Z};
  static constexpr uint64_t kMontgomeryMPrime = ~uint64_t(0);
  static constexpr uint64_t kGenerator = 7;
  static constexpr ValueType kMaxDivisible =
      0xf8000000000000000000000000000000000000000007c000000000000000001f_Z;
};

template <>
struct BigPrimeConstants<255, 4> {
  using ValueType = BigInt<4>;

  static constexpr ValueType kModulus =
      0x73eda753299d7d483339d80809a1d80553bda402fffe5bfeffffffff00000001_Z;
  static constexpr ValueType kMontgomeryR =
      0x1824b159acc5056f998c4fefecbc4ff55884b7fa0003480200000001fffffffe_Z;
  static constexpr ValueType kMontgomeryRSquared =
      0x0748d9d99f59ff1105d314967254398f2b6cedcb87925c23c999e990f3f29c6d_Z;
  static constexpr ValueType kMontgomeryRCubed =
      0x6e2a5bb9c8db33e973d13c71c7b5f4181b3e0d188cf06990c62c1807439b73af_Z;
  static constexpr std::array<ValueType, 12> kFactors{
      0x2_Z,     0x3_Z,     0xb_Z,      0x13_Z,     0x27c1_Z,    0x1ea57_Z,
      0xd1c83_Z, 0xdd46d_Z, 0x264679_Z, 0x26987b_Z, 0x320238b_Z, 0xf2f5565_Z};
  static constexpr uint64_t kMontgomeryMPrime = 18446744069414584319UL;
  static constexpr uint64_t kGenerator = 7;
  static constexpr ValueType kMaxDivisible =
      0xe7db4ea6533afa906673b0101343b00aa77b4805fffcb7fdfffffffe00000002_Z;
};

template <>
struct BigPrimeConstants<124, 5> {
  using ValueType = BigInt<2>;

  static constexpr ValueType kModulus = 0x8000000000000aa0000000000000001_Z;
  static constexpr ValueType kMontgomeryR = 0x7ffffffffffeb69ffffffffffffffe1_Z;
  static constexpr ValueType kMontgomeryRSquared = 0x7ffffda845150a9ffffffffc78e0401_Z;
  static constexpr ValueType kMontgomeryRCubed = 0x5e20f0c990105f9ff9c6f7f0abf8009_Z;
  static constexpr std::array<ValueType, 5> kFactors{0x2_Z, 0xb_Z, 0x1f_Z, 0x6d3_Z, 0x70a67e525b_Z};
  static constexpr uint64_t kMontgomeryMPrime = 18446744073709551615UL;
  static constexpr uint64_t kGenerator = 6;
  static constexpr ValueType kMaxDivisible = 0xf800000000001496000000000000001f_Z;
};

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_FIELDS_BIG_PRIME_CONSTANTS_H_
