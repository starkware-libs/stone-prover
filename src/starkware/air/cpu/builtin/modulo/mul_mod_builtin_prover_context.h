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

#ifndef STARKWARE_AIR_CPU_BUILTIN_MODULO_MUL_MOD_BUILTIN_PROVER_CONTEXT_H_
#define STARKWARE_AIR_CPU_BUILTIN_MODULO_MUL_MOD_BUILTIN_PROVER_CONTEXT_H_

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/memory/memory.h"
#include "starkware/air/components/perm_range_check/range_check_cell.h"
#include "starkware/air/cpu/builtin/modulo/mod_builtin_prover_context.h"
#include "starkware/math/math.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT, size_t NWords>
class MulModBuiltinProverContext : public ModBuiltinProverContext<FieldElementT, NWords> {
 public:
  using ValueType = typename FieldElementT::ValueType;
  using BigInteger = typename ModBuiltinProverContext<FieldElementT, NWords>::BigInteger;
  using BigIntegerMult = typename ModBuiltinProverContext<FieldElementT, NWords>::BigIntegerMult;
  using Input = typename ModBuiltinProverContext<FieldElementT, NWords>::Input;
  using BatchSlice = typename ModBuiltinProverContext<FieldElementT, NWords>::BatchSlice;
  using RCColumns = typename std::vector<std::vector<TableCheckCellView<FieldElementT>>>;

  MulModBuiltinProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>* memory_pool, RangeCheckCell<FieldElementT>* rc_pool,
      const uint64_t begin_addr, const size_t n_instances, const size_t batch_size,
      const size_t word_bit_len, const size_t bits_per_part,
      const std::map<uint64_t, Input>& inputs)
      : ModBuiltinProverContext<FieldElementT, NWords>(
            name, ctx, memory_pool, begin_addr, n_instances, batch_size, word_bit_len,
            std::move(inputs)),
        n_carry_words_(2 * (NWords - 1)),
        bits_per_part_(bits_per_part),
        p_multiplier_(InitRCColumns(
            rc_pool, name + "/p_multiplier", ctx, NWords, SafeDiv(word_bit_len, bits_per_part))),
        carry_(InitRCColumns(
            rc_pool, name + "/carry", ctx, n_carry_words_,
            DivCeil(word_bit_len + Log2Ceil(NWords) + 1, bits_per_part))) {}

  /*
    Writes the trace cells for the builtin.
  */
  void WriteTrace(gsl::span<const gsl::span<FieldElementT>> trace) const;

 private:
  /*
    Initializes a 2d-vector of shape (n_words, n_parts) of rc_pool views.
  */
  static const RCColumns InitRCColumns(
      RangeCheckCell<FieldElementT>* rc_pool, const std::string& name,
      const TraceGenerationContext& ctx, const size_t n_words, const size_t n_parts) {
    RCColumns res;
    res.reserve(n_words);
    for (size_t i = 0; i < n_words; ++i) {
      res.push_back({});
      res.back().reserve(n_parts);
      for (size_t j = 0; j < n_parts; ++j) {
        res.back().emplace_back(TableCheckCellView<FieldElementT>(
            rc_pool, name + std::to_string(i) + "/part" + std::to_string(j), ctx));
      }
    }
    return res;
  }
  /*
    Performs an arithmetic right shift by <word_bit_len> bits to convert the sum
    of one column of partial products to the carry for the next column.
  */
  ValueType UnshiftCarry(const ValueType& carry) const;
  /*
    Used in WriteTrace to write the trace cell for one carry word.
  */
  void WriteCarry(
      gsl::span<const gsl::span<FieldElementT>> trace, const ValueType& shifted_carry,
      const size_t index_1d, const size_t word) const;

  void WriteRC(
      const std::vector<TableCheckCellView<FieldElementT>>& rc_view, ValueType input,
      gsl::span<const gsl::span<FieldElementT>> trace, const size_t index_1d) const;

  const size_t n_carry_words_;
  const size_t bits_per_part_;
  /*
    RC_pool columns.
  */
  const RCColumns p_multiplier_;
  const RCColumns carry_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/builtin/modulo/mul_mod_builtin_prover_context.inl"

#endif  // STARKWARE_AIR_CPU_BUILTIN_MODULO_MUL_MOD_BUILTIN_PROVER_CONTEXT_H_
