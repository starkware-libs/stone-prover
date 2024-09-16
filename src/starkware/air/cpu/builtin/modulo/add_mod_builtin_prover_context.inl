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

#include "starkware/math/math.h"
#include "starkware/utils/task_manager.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT, size_t NWords>
void AddModBuiltinProverContext<FieldElementT, NWords>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  for (size_t inst = 0; inst < this->n_instances_; ++inst) {
    const Input& input = this->WriteInput(trace, inst);
    BigInteger p = BigInteger::Zero();
    // Convert p from vector<ValueType>(NWords) to one BigInteger.
    for (size_t word = 0; word < NWords; ++word) {
      p = p | (BigInteger(input.p[word]) << (word * this->word_bit_len_));
    }
    for (size_t ind = 0; ind < this->batch_size_; ++ind) {
      const size_t index_1d = inst * this->batch_size_ + ind;
      BigInteger a = BigInteger::Zero();
      BigInteger b = BigInteger::Zero();
      BigInteger c = BigInteger::Zero();
      // Convert a, b, c from vector<ValueType>(NWords) to BigIntegers.
      for (size_t word = 0; word < NWords; ++word) {
        a = a | (BigInteger(input.batch[ind].a[word]) << (word * this->word_bit_len_));
        b = b | (BigInteger(input.batch[ind].b[word]) << (word * this->word_bit_len_));
        c = c | (BigInteger(input.batch[ind].c[word]) << (word * this->word_bit_len_));
      }
      const auto args_str_gen = [&]() {
        return "p = " + p.ToString() + ", a = " + a.ToString() + ", b = " + b.ToString() +
               ", c = " + c.ToString();
      };
      auto a_plus_b = BigInteger::Add(a, b);
      auto sub_p_bit = 0;
      if (a_plus_b == std::make_pair(c, false)) {
        // Computation does not exceed BigInteger, a + b = c (sub_p_bit is zero).
        sub_p_bit_.SetCell(trace, index_1d, FieldElementT::Zero());
      } else {
        // a + b = c + p (sub_p_bit is one).
        ASSERT_RELEASE(
            a_plus_b == BigInteger::Add(c, p),
            "Invalid input: a + b != c (mod p). " + args_str_gen());
        sub_p_bit_.SetCell(trace, index_1d, FieldElementT::One());
        sub_p_bit = 1;
      }
      ValueType signed_carry_bit = ValueType::Zero();
      for (size_t word = 0; word < NWords - 1; ++word) {
        ValueType carry = input.batch[ind].a[word] + input.batch[ind].b[word] -
                          input.batch[ind].c[word] + signed_carry_bit;
        if (sub_p_bit == 1) {
          carry = carry - input.p[word];
        }
        signed_carry_bit = WriteCarry(trace, carry, index_1d, word, args_str_gen);
      }
      ASSERT_RELEASE(
          input.batch[ind].a[NWords - 1] + input.batch[ind].b[NWords - 1] -
                  input.batch[ind].c[NWords - 1] -
                  (sub_p_bit ? input.p[NWords - 1] : ValueType::Zero()) + signed_carry_bit ==
              ValueType::Zero(),
          "Error: a + b != c (mod p). " + args_str_gen());
    }
  }
}

template <typename FieldElementT, size_t NWords>
typename FieldElementT::ValueType AddModBuiltinProverContext<FieldElementT, NWords>::WriteCarry(
    gsl::span<const gsl::span<FieldElementT>> trace, const ValueType& carry, const size_t index_1d,
    const size_t word, const std::function<std::string()>& args_str_gen) const {
  const ValueType shift = ValueType::One() << this->word_bit_len_;
  if (carry == ValueType::Zero()) {
    carry_bit_[word].SetCell(trace, index_1d, FieldElementT::Zero());
    carry_sign_[word].SetCell(trace, index_1d, FieldElementT::One());
    return ValueType::Zero();
  } else if (carry == shift) {
    carry_bit_[word].SetCell(trace, index_1d, FieldElementT::One());
    carry_sign_[word].SetCell(trace, index_1d, FieldElementT::One());
    return ValueType::One();
  } else {
    ASSERT_RELEASE(carry == -shift, "Invalid input: carry is not -1, 0 or 1. " + args_str_gen());
    carry_bit_[word].SetCell(trace, index_1d, FieldElementT::One());
    carry_sign_[word].SetCell(trace, index_1d, -FieldElementT::One());
    return -ValueType::One();
  }
}

}  // namespace cpu
}  // namespace starkware
