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

#include <iomanip>
#include <ios>
#include <limits>
#include <tuple>

#include "starkware/algebra/uint128.h"
#include "starkware/utils/attributes.h"
#include "starkware/utils/serialization.h"

namespace starkware {

template <size_t N>
constexpr uint64_t BigInt<N>::AsUint() noexcept {
  for (auto it = value_.begin() + 1; it != value_.end(); ++it) {
    ASSERT_RELEASE(*it == 0, "Value is too large for casting to uint64_t.");
  }
  return value_[0];
}

template <size_t N>
BigInt<N> BigInt<N>::RandomBigInt(PrngBase* prng) {
  std::array<std::byte, N * sizeof(uint64_t)> bytes{};
  prng->GetRandomBytes(bytes);
  return FromBytes(bytes);
}

template <size_t N>
template <size_t K>
constexpr BigInt<N>::BigInt(const BigInt<K>& v) noexcept : value_{} {
  static_assert(N > K, "trimming is not supported");
  for (size_t i = 0; i < K; ++i) {
    constexpr_at(value_, i) = v[i];
  }

  for (size_t i = K; i < N; ++i) {
    constexpr_at(value_, i) = 0;
  }
}

template <size_t N>
template <size_t K>
constexpr BigInt<N> BigInt<N>::FromBigInt(const BigInt<K>& other) {
  std::array<uint64_t, N> v{};
  if constexpr (N >= K) {  // NOLINT: clang tidy if constexpr bug.
    return other;
  }
  for (size_t i = 0; i < N; ++i) {
    v.at(i) = other[i];
  }

  for (size_t i = N; i < K; ++i) {
    ASSERT_RELEASE(other[i] == 0, "Number too big to be trimmed.");
  }

  return BigInt<N>(v);
}

template <size_t N>
constexpr std::pair<BigInt<N>, bool> BigInt<N>::Add(const BigInt& a, const BigInt& b) {
  bool carry{};
  BigInt r{0};

  for (size_t i = 0; i < N; ++i) {
    Uint128 res = static_cast<Uint128>(a[i]) + b[i] + carry;
    carry = (res >> 64) != static_cast<Uint128>(0);
    r[i] = static_cast<uint64_t>(res);
  }

  return std::make_pair(r, carry);
}

template <size_t N>
constexpr BigInt<2 * N> BigInt<N>::operator*(const BigInt<N>& other) const {
  constexpr auto kResSize = 2 * N;
  BigInt<kResSize> final_res = BigInt<kResSize>::Zero();
  // Multiply this by other using long multiplication algorithm.
  for (size_t i = 0; i < N; ++i) {
    uint64_t carry = static_cast<uint64_t>(0U);
    for (size_t j = 0; j < N; ++j) {
      // For M == UINT64_MAX, we have: a*b+c+d <= M*M + 2M = (M+1)^2 - 1 == UINT128_MAX.
      // So we can do a multiplication and an addition without an overflow.
      Uint128 res = Umul128((*this)[j], other[i]) + final_res[i + j] + carry;
      carry = gsl::narrow_cast<uint64_t>(res >> 64);
      final_res[i + j] = gsl::narrow_cast<uint64_t>(res);
    }
    final_res[i + N] = static_cast<uint64_t>(carry);
  }
  return final_res;
}

template <size_t N>
BigInt<N> BigInt<N>::MulMod(const BigInt& a, const BigInt& b, const BigInt& modulus) {
  const BigInt<2 * N> mul_res = a * b;
  const BigInt<2 * N> mul_res_mod = BigInt<2 * N>::Div(mul_res, BigInt<2 * N>(modulus)).second;

  BigInt<N> res = Zero();

  // Trim mul_res_mod to the N lower limbs (this is possible since it must be smaller than modulus).
  for (size_t i = 0; i < N; ++i) {
    res[i] = mul_res_mod[i];
  }

  return res;
}

template <size_t N>
BigInt<N> BigInt<N>::AddMod(const BigInt& a, const BigInt& b, const BigInt& modulus) {
  const auto& [res, carry] = Add(Div(a, modulus).second, Div(b, modulus).second);
  if (carry || res >= modulus) {
    return res - modulus;
  }
  return res;
}

template <size_t N>
inline void BigInt<N>::operator^=(const BigInt& other) {
  for (size_t i = 0; i < N; ++i) {
    constexpr_at(value_, i) ^= other[i];
  }
}

template <size_t N>
constexpr std::pair<BigInt<N>, bool> BigInt<N>::Sub(const BigInt& a, const BigInt& b) {
  bool carry{};
  BigInt r{};

  for (size_t i = 0; i < N; ++i) {
    Uint128 res = static_cast<Uint128>(a[i]) - b[i] - carry;
    carry = (res >> 127) != static_cast<Uint128>(0);
    r[i] = static_cast<uint64_t>(res);
  }

  return std::make_pair(r, carry);
}

template <size_t N>
constexpr bool BigInt<N>::operator<(const BigInt& b) const {
  return Sub(*this, b).second;
}

template <size_t N>
std::pair<BigInt<N>, BigInt<N>> BigInt<N>::Div(BigInt a, const BigInt& b) {
  ASSERT_RELEASE(b != BigInt::Zero(), "Divisor must not be zero.");
  bool carry{};
  BigInt res{};
  BigInt shifted_b{}, tmp{};

  while (a >= b) {
    tmp = b;
    int shift = -1;
    do {
      shifted_b = tmp;
      shift++;
      std::tie(tmp, carry) = Add(shifted_b, shifted_b);
    } while (!carry && tmp <= a);

    a = Sub(a, shifted_b).first;
    res[shift / 64] |= Pow2(shift % 64);
  }

  return std::make_pair(res, a);
}

template <size_t N>
constexpr BigInt<N> BigInt<N>::Inverse(const BigInt& value, const BigInt& modulus) {
  bool carry{};
  BigInt shifted_val{}, shifted_coef{}, tmp{};
  struct {
    BigInt val{};
    BigInt coef{};
  } u, v;

  u.val = value;
  u.coef = BigInt::One();

  v.val = modulus;
  v.coef = BigInt::Zero();

  while (BigInt::One() < v.val) {
    if (u.val >= v.val) {
      const auto tmp = u;
      u = v;
      v = tmp;
    }

    shifted_coef = u.coef;
    shifted_val = u.val;
    while (true) {
      // The loop maintains the invariant that v.val = v.coeff * value (mod modulus).
      {
        const auto sum_and_carry = Add(shifted_val, shifted_val);
        tmp = sum_and_carry.first;
        carry = sum_and_carry.second;
      }
      if (carry || tmp >= v.val) {
        break;
      }
      shifted_val = tmp;
      {
        const auto sum_and_carry = Add(shifted_coef, shifted_coef);
        shifted_coef = sum_and_carry.first;
        carry = sum_and_carry.second;
      }
      if (carry || shifted_coef >= modulus) {
        shifted_coef = Sub(shifted_coef, modulus).first;
      }
    }

    v.val = Sub(v.val, shifted_val).first;
    const auto diff_and_carry = Sub(v.coef, shifted_coef);
    v.coef = diff_and_carry.first;
    carry = diff_and_carry.second;
    if (carry) {
      v.coef = Add(v.coef, modulus).first;
    }
  }

  ASSERT_RELEASE(
      v.val == BigInt::One(),
      "GCD(value,modulus) is not 1, in particular, the value is not invertable");
  return v.coef;
}

template <size_t N>
constexpr bool BigInt<N>::IsEven() const {
  // Check the LSB of the number.
  return (value_[0] & 1) == 0;
}

template <size_t N>
constexpr bool BigInt<N>::IsMsbSet() const {
  // Check the MSB of the number.
  return ((value_[N - 1] & Pow2(std::numeric_limits<uint64_t>::digits - 1)) != 0);
}

template <size_t N>
BigInt<N> BigInt<N>::FromBytes(gsl::span<const std::byte> bytes, bool use_big_endian) {
  return Deserialize<BigInt>(bytes, use_big_endian);
}

template <size_t N>
void BigInt<N>::ToBytes(gsl::span<std::byte> span_out, bool use_big_endian) const {
  Serialize(*this, span_out, use_big_endian);
}

template <size_t N>
BigInt<N> BigInt<N>::FromString(const std::string& s) {
  // Note: this function assumes that FromBytes() uses little endian encoding.
  std::array<std::byte, BigInt<N>::SizeInBytes()> as_bytes{};
  HexStringToBytes(s, as_bytes);
  return Deserialize<BigInt>(as_bytes, /*use_big_endian=*/true);
}

template <size_t N>
std::string BigInt<N>::ToString() const {
  std::array<std::byte, BigInt<N>::SizeInBytes()> as_bytes{};
  Serialize(*this, as_bytes, /*use_big_endian=*/true);
  return BytesToHexString(as_bytes);
}

template <size_t N>
std::vector<bool> BigInt<N>::ToBoolVector() const {
  std::vector<bool> res;
  for (uint64_t value : value_) {
    for (int i = 0; i < std::numeric_limits<uint64_t>::digits; ++i) {
      res.push_back((value & 1) != 0);
      value >>= 1;
    }
  }
  return res;
}

template <size_t N>
auto BigInt<N>::ToNibbleArray() const -> std::array<uint8_t, kNibbles> {
  constexpr size_t kNibblesPerValue = SafeDiv(kNibbles, N);
  std::array<uint8_t, kNibbles> res{};
  size_t dig_index = 0;

  for (uint64_t value : value_) {
    for (size_t i = 0; i < kNibblesPerValue; ++i, ++dig_index) {
      res.at(dig_index) = value & 15;
      value >>= 4;
    }
  }
  return res;
}

template <size_t N>
BigInt<N> BigInt<N>::operator&(const BigInt<N>& other) const {
  BigInt res = other;
  for (size_t i = 0; i < N; ++i) {
    res.value_.at(i) &= value_.at(i);
  }
  return res;
}

template <size_t N>
BigInt<N> BigInt<N>::operator^(const BigInt<N>& other) const {
  BigInt res = *this;
  res ^= other;
  return res;
}

template <size_t N>
BigInt<N> BigInt<N>::operator|(const BigInt<N>& other) const {
  BigInt res = other;
  for (size_t i = 0; i < N; ++i) {
    res.value_.at(i) |= value_.at(i);
  }
  return res;
}

template <size_t N>
constexpr bool BigInt<N>::operator==(const BigInt<N>& other) const {
  for (size_t i = 0; i < N; ++i) {
    if (gsl::at(value_, i) != gsl::at(other.value_, i)) {
      return false;
    }
  }
  return true;
}

template <size_t N>
void BigInt<N>::RightShiftWords(const size_t shift) {
  const size_t fixed_shift = std::min(shift, N);
  std::move(value_.begin() + fixed_shift, value_.end(), value_.begin());
  std::fill(value_.begin() + N - fixed_shift, value_.end(), 0);
}

template <size_t N>
void BigInt<N>::LeftShiftWords(const size_t shift) {
  const size_t fixed_shift = std::min(shift, N);
  std::move(value_.rbegin() + fixed_shift, value_.rend(), value_.rbegin());
  std::fill(value_.rbegin() + N - fixed_shift, value_.rend(), 0);
}

template <size_t N>
inline BigInt<N>& BigInt<N>::operator>>=(const size_t shift) {
  const size_t n_bits_word = sizeof(uint64_t) * 8;
  const size_t n_words_shift = shift / n_bits_word;
  const size_t n_bits_shift = shift % n_bits_word;

  // Shift entire words.
  if (n_words_shift != 0) {
    RightShiftWords(n_words_shift);
  }

  // Shift bits internally if needed.
  if (n_bits_shift == 0) {
    return *this;
  }

  const size_t fix_prev_shift = n_bits_word - n_bits_shift;
  const uint64_t mask = Pow2(n_bits_shift) - 1;

  value_[0] >>= n_bits_shift;
  for (size_t i = 1; i < value_.size(); ++i) {
    value_.at(i - 1) ^= (value_.at(i) & mask) << fix_prev_shift;
    value_.at(i) >>= n_bits_shift;
  }
  return *this;
}

template <size_t N>
constexpr BigInt<N>& BigInt<N>::operator<<=(const size_t shift) {
  const size_t n_bits_word = sizeof(uint64_t) * 8;
  const size_t n_words_shift = shift / n_bits_word;
  const size_t n_bits_shift = shift % n_bits_word;

  // Shift entire words.
  if (n_words_shift != 0) {
    LeftShiftWords(n_words_shift);
  }

  // Shift bits internally if needed.
  if (n_bits_shift == 0) {
    return *this;
  }

  const size_t fix_prev_shift = n_bits_word - n_bits_shift;
  const uint64_t mask = ~(Pow2(64 - n_bits_shift) - 1);

  for (size_t i = value_.size() - 1; i > 0; i--) {
    value_.at(i) <<= n_bits_shift;
    value_.at(i) ^= (value_.at(i - 1) & mask) >> fix_prev_shift;
  }
  value_.at(0) <<= n_bits_shift;
  return *this;
}

template <size_t N>
BigInt<N> BigInt<N>::GetWithRegisterHint() const {
  BigInt res(*this);
#ifdef USE_REGISTER_HINTS
  for (uint64_t& limb : res.value_) {
    // Instruct the compiler to put the limb in a register and tells the compiler that the
    // limb might be modified.
    // Since the limb is potentially modified, the compiler is forced to use the value
    // from the register rather than the original value that could be a compile time constant.
    // The compiler is allowed to spill the limb back to memory following the asm block
    // but it usually tries to avoid spilling.
    asm("" : "+r"(limb));
  }
#endif
  return res;
}

template <size_t N>
template <bool IsConstexpr>
constexpr BigInt<N> BigInt<N>::ReduceIfNeeded(const BigInt<N>& x, const BigInt<N>& target) {
  ASSERT_DEBUG(target.NumLeadingZeros() > 0, "target must have at least one leading zero.");

  // To get efficient machine code we want the compiler to evaluate -modulus in compile time
  // and then put the result in a register.
  auto minus_target = -target;
  if constexpr (!IsConstexpr) {  // NOLINT: clang tidy if constexpr bug.
    // GetWithRegisterHint() calls to assembly code and cannot be invoked during compilation.
    minus_target = minus_target.GetWithRegisterHint();
  }
  auto reduced_candidate = minus_target + x;

  // 0 <= x < 2*target ---> -target <= reduced_candidate < target.
  // So assuming target.NumLeadingZeros() > 0, we can use the reduced_candidate.IsMsbSet() to
  // determine the sign of reduced_candidate.

  // The code bellow is used to encourage the compiler to use cmov.
  for (size_t i = 0; i < N; ++i) {
    reduced_candidate[i] = reduced_candidate.IsMsbSet() ? x[i] : reduced_candidate[i];
  }

  return reduced_candidate;
}

template <size_t N>
BigInt<N> BigInt<N>::OneLimbMontgomeryReduction(
    const BigInt& x, const BigInt& modulus, uint64_t montgomery_mprime) {
  BigInt res{};
  uint64_t u_i = x[0] * montgomery_mprime;
  uint64_t carry = 0;

  for (size_t j = 0; j < N; ++j) {
    Uint128 temp = Umul128(modulus[j], u_i) + x[j] + carry;
    res[j] = gsl::narrow_cast<uint64_t>(temp);
    carry = gsl::narrow_cast<uint64_t>(temp >> 64);
  }

  for (size_t j = 0; j < N - 1; ++j) {
    res[j] = res[j + 1];
  }

  res[N - 1] = carry;
  // Note that both modulus * u_i and x are less than modulus * 2^64 therfore res is guaranteed
  // to be less than 2*modulus and we can use ReduceIfNeeded to get a non redundant representation.
  return ReduceIfNeeded(res, modulus);
}

template <size_t N>
ALWAYS_INLINE constexpr BigInt<N> BigInt<N>::MontMul(
    const BigInt& x, const BigInt& y, const BigInt& modulus, uint64_t montgomery_mprime) {
  BigInt<N> res{};

  // We require (y + modulus) to fit in 64N bits to avoid carry between Montgomery rounds.
  // We enforce this with the following asserts:
  ASSERT_DEBUG(
      modulus.NumLeadingZeros() > 0, "We require at least one leading zero in the modulus");
  ASSERT_DEBUG(y < modulus, "y is supposed to be smaller than the modulus");

  for (size_t i = 0; i < N; ++i) {
    Uint128 temp = Umul128(x[i], y[0]) + res[0];
    uint64_t u_i = gsl::narrow_cast<uint64_t>(temp) * montgomery_mprime;
    uint64_t carry1 = 0, carry2 = 0;

    // Each outer loop iteration does
    // res = (res + x[i] * y + u_i * modulus) >> 64
    // if res above doesn't fit in BigInt we call field_reducer to
    // find a congurent value mod modulus that does fit in BigInt
    // we don't lose any information since:
    // (res + x[i] * y + u_i * kModulus) == 0 mod 2^64
    // res + x[i] * y + (res + x[i] * y  mod 2^64) *montgomery_mprime *kModulus
    //
    // To understand, denote
    // a := res + x[i] * y mod(2^64)
    // Note that u_i = montgomery_mprime * a mod(2^64)
    // montgomery_mprime * kModulus = -1 mod(2^64)
    // and therefore
    // (res + x[i] * y + u_i * kModulus) = (a) + (a) * (-1) = 0 mod(2^64).

    for (size_t j = 0; j < N; ++j) {
      // Note that if we have M == UINT64_MAX
      // a*b+c+d <= M*M + 2M = (M+1)^2 - 1 == UINT128_MAX.
      // So we can do a multiplication and two additions without an overflow.

      // We use the observation above twice in the following code.
      // We further break each 128 bit addition to two 64 bit additions because
      // at the time of this writing this approach resulted in better machine code.

      // [carry1:low] = Umul128(x[i], y[j]) + res[j] + carry1;
      if (j != 0) {
        temp = Umul128(x[i], y[j]) + res[j];
      }
      uint64_t low = carry1 + gsl::narrow_cast<uint64_t>(temp);
      carry1 = gsl::narrow_cast<uint64_t>(temp >> 64) + static_cast<uint64_t>(low < carry1);

      // [carry2:res[j]] = Umul128(modulus[j], u_i) + carry2 + low;
      temp = Umul128(modulus[j], u_i) + carry2;
      res[j] = low + gsl::narrow_cast<uint64_t>(temp);
      carry2 = gsl::narrow_cast<uint64_t>(temp >> 64) + static_cast<uint64_t>(res[j] < low);

      // We pass on both carry1 and carry2. We can't just add them here
      // as the addition might not fit in 64 bit.
    }

    // The shift could be avoided if we stored the result in a cyclic manner
    // I.e. iteration i will use res[i+3%4]:res[i+2%4]:res[i+1%4]:res[i%4]
    // but it looks like clang does it on its own when optimizations are enabled.
    for (size_t j = 0; j < N - 1; ++j) {
      res[j] = res[j + 1];
    }

    res[N - 1] = carry1 + carry2;
    // The i'th iteration of the outer loop calculates (y * x_i+ modulus * u_i) / 2^64.
    // (y * x_i + modulus * u_i) / 2^64 <= (y + modulus) * (2^64-1) / 2^64 < y + modulus.
    // So assuming (y + modulus) fit in 64N bits there won't be a carry here.

    ASSERT_DEBUG(res[N - 1] >= carry1, "There shouldn't be a carry here.");
  }
  return res;
}

template <size_t N>
constexpr size_t BigInt<N>::NumLeadingZeros() const {
  int i = value_.size() - 1;
  size_t res = 0;

  while (i >= 0 && (gsl::at(value_, i) == 0)) {
    i--;
    res += std::numeric_limits<uint64_t>::digits;
  }

  if (i >= 0) {
    res += __builtin_clzll(gsl::at(value_, i));
  }

  return res;
}

template <size_t N>
constexpr size_t BigInt<N>::Log2Floor() const {
  size_t leading_zeros = NumLeadingZeros();

  ASSERT_RELEASE(kDigits > leading_zeros, "log2 of 0 is undefined");
  return kDigits - 1 - NumLeadingZeros();
}

template <size_t N>
constexpr size_t BigInt<N>::Log2Ceil() const {
  // Not optimized implementation, can be implemented using a single pass, instead of two passes
  // as done here.
  return Log2Floor() + (IsPowerOfTwo() ? 0 : 1);
}

template <size_t N>
constexpr bool BigInt<N>::IsPowerOfTwo() const {
  size_t n_pow_two = 0;
  size_t n_no_zero = 0;

  for (size_t i = 0; i < value_.size(); ++i) {
    if (gsl::at(value_, i) != 0) {
      n_no_zero++;
      if (starkware::IsPowerOfTwo(gsl::at(value_, i))) {
        n_pow_two++;
      }
    }
  }

  return (n_pow_two == 1) && (n_no_zero == 1);
}

template <size_t N>
std::ostream& operator<<(std::ostream& os, const BigInt<N>& bigint) {
  return os << bigint.ToString();
}

namespace bigint {
namespace details {
/*
  Converts an hex digit ASCII char to the corresponding int.
  Assumes the input is an hex digit.
*/
constexpr uint64_t HexCharToUint64(char c) {
  if ('0' <= c && c <= '9') {
    return c - '0';
  }

  if ('A' <= c && c <= 'F') {
    return c - 'A' + 10;
  }

  // The function assumes that the input is an hex digit, so we can assume 'a' <= c && c <= 'f'
  // here.
  return c - 'a' + 10;
}

template <char... Chars>
constexpr auto HexCharArrayToBigInt() {
  constexpr size_t kLen = sizeof...(Chars);
  constexpr std::array<char, kLen> kDigits{Chars...};
  static_assert(kDigits[0] == '0' && kDigits[1] == 'x', "Only hex input is currently supported");

  constexpr size_t kNibblesPerUint64 = 2 * sizeof(uint64_t);
  constexpr size_t kResLen = (kLen - 2 + kNibblesPerUint64 - 1) / (kNibblesPerUint64);
  std::array<uint64_t, kResLen> res{};

  for (size_t i = 0; i < kDigits.size() - 2; ++i) {
    const size_t limb = i / kNibblesPerUint64;
    const size_t nibble_offset = i % kNibblesPerUint64;
    const uint64_t nibble = HexCharToUint64(gsl::at(kDigits, kDigits.size() - i - 1));

    constexpr_at(res, limb) |= nibble << (4 * nibble_offset);
  }

  return BigInt<res.size()>(res);
}
}  // namespace details
}  // namespace bigint

template <char... Chars>
static constexpr auto operator"" _Z() {
  // This function is implemented as wrapper that calls the actual implementation and
  // stores it in a constexpr variable as we want to force the evaluation to be done in compile
  // time.
  // We need to have the function call because "constexpr auto kRes = BigInt<res.size()>(res);"
  // won't work unless res is constexpr.

  // Note that the compiler allows HEX and decimal literals but in any case
  // it enforces that Chars... contains only HEX (or decimal) characters.
  constexpr auto kRes = bigint::details::HexCharArrayToBigInt<Chars...>();
  return kRes;
}

}  // namespace starkware
