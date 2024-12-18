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

#include "starkware/utils/task_manager.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT, size_t NWords>
void MulModBuiltinProverContext<FieldElementT, NWords>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  const BigInteger MASK = (BigInteger::One() << this->word_bit_len_) - BigInteger::One();
  for (size_t inst = 0; inst < this->n_instances_; ++inst) {
    const Input& input = this->WriteInput(trace, inst);
    BigInteger p = BigInteger::Zero();
    // Convert p from vector<ValueType>(NWords) to one BigInteger.
    for (size_t word = 0; word < NWords; ++word) {
      p = p | (BigInteger(input.p[word]) << (word * this->word_bit_len_));
    }
    BigIntegerMult big_p(p);
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
      auto a_times_b_sub_c = a * b - BigIntegerMult(c);
      auto [p_multiplier, rem] =  // NOLINT (auto [...]).
          p == BigInteger::Zero() ? std::make_pair(BigIntegerMult::Zero(), BigIntegerMult::Zero())
                                  : a_times_b_sub_c.Div(big_p);
      ASSERT_RELEASE(
          rem == BigIntegerMult::Zero(), "Invalid input: a * b != c (mod p). " + args_str_gen());
      BigInteger p_multiplier_reduced = BigInteger::FromBigInt(p_multiplier);
      std::vector<ValueType> p_multiplier_vec(NWords);
      for (size_t word = 0; word < NWords; ++word) {
        p_multiplier_vec[word] =
            ValueType::FromBigInt((p_multiplier_reduced >> (word * this->word_bit_len_)) & MASK);
        WriteRC(p_multiplier_[word], p_multiplier_vec[word], trace, index_1d);
      }

      ValueType carry = ValueType::Zero();
      // We compute the carries of the computation a * b - p_multiplier * p - c, which should
      // equal 0, word by word.
      for (size_t word = 0; word < n_carry_words_; ++word) {
        // The products contributing to term `word` are from index pairs (i, j) of (a, b) and
        // (p_multipler, p) such that i + j = word and 0 <= i,j < NWords. Equivalently, these are
        // pairs (i, word - i) such that 0 <= i < NWords and (word - NWords) < i <=  word. The
        // intersection of these two interval conditions is simplified below as
        //     lower := max(word - NWords + 1, 0) <= i < min(word + 1, NWords) =: upper.
        size_t lower = std::max(word + 1, NWords) - NWords, upper = std::min(word + 1, NWords);
        for (size_t i = lower; i < upper; ++i) {
          carry += ValueType::FromBigInt(input.batch[ind].a[i] * input.batch[ind].b[word - i]);
          carry -= ValueType::FromBigInt(p_multiplier_vec[i] * input.p[word - i]);
        }
        // The result c only contributes to the first NWords terms, as it is generally shorter
        // than the full product.
        if (word < NWords) {
          carry -= input.batch[ind].c[word];
        }

        carry = UnshiftCarry(carry);
        WriteCarry(trace, carry, index_1d, word);
      }

      // We expect the result of a*b - p_multiplier*p - c to be zero, so the most significant
      // partial product (the one involving the most significant words of a, b, p and p_multiplier)
      // shouldn't have any carry coming out of it.
      ASSERT_RELEASE(
          (NWords == 1 ? input.batch[ind].c[NWords - 1] : ValueType::Zero()) ==
              ValueType::FromBigInt(
                  input.batch[ind].a[NWords - 1] * input.batch[ind].b[NWords - 1]) -
                  ValueType::FromBigInt(input.p[NWords - 1] * p_multiplier_vec[NWords - 1]) + carry,
          "Error: a * b != c (mod p). " + args_str_gen());
    }
  }
}

template <typename FieldElementT, size_t NWords>
typename MulModBuiltinProverContext<FieldElementT, NWords>::ValueType
MulModBuiltinProverContext<FieldElementT, NWords>::UnshiftCarry(const ValueType& carry) const {
  const ValueType mask = (ValueType::One() << this->word_bit_len_) - ValueType::One();
  ASSERT_RELEASE(
      (carry & mask) == ValueType::Zero(), "Invalid input: carry is not divisible by shift. ");

  // The following performs arithmetic (that is, sign-extending) right shift. We split
  // to negative-carry and positive-carry cases because our ValueType (BigInt) only
  // supports logical right shifts.
  if (carry.IsMsbSet()) {
    return -((-carry) >> this->word_bit_len_);
  } else {
    return carry >> this->word_bit_len_;
  }
}

template <typename FieldElementT, size_t NWords>
void MulModBuiltinProverContext<FieldElementT, NWords>::WriteCarry(
    gsl::span<const gsl::span<FieldElementT>> trace, const ValueType& carry, const size_t index_1d,
    const size_t word) const {
  const ValueType carry_offset = ValueType(NWords) << this->word_bit_len_;
  const ValueType carry_to_write = carry + carry_offset;
  ASSERT_RELEASE(
      !carry_to_write.IsMsbSet(), "After adding the offset, the carry should be positive");
  WriteRC(carry_[word], carry_to_write, trace, index_1d);
}

template <typename FieldElementT, size_t NWords>
void MulModBuiltinProverContext<FieldElementT, NWords>::WriteRC(
    const std::vector<TableCheckCellView<FieldElementT>>& rc_view, ValueType input,
    gsl::span<const gsl::span<FieldElementT>> trace, const size_t index_1d) const {
  const uint64_t mask = (uint64_t(1) << bits_per_part_) - 1;
  for (size_t part = 0; part < rc_view.size(); ++part) {
    rc_view[part].WriteTrace(index_1d, input[0] & mask, trace);
    input >>= bits_per_part_;
  }
  ASSERT_RELEASE(
      input == ValueType::Zero(),
      "Error: Intermediate value in computation exceeds rc_pool allocation.");
}

}  // namespace cpu
}  // namespace starkware
