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

#ifndef STARKWARE_AIR_CPU_BUILTIN_MODULO_ADD_MOD_BUILTIN_PROVER_CONTEXT_H_
#define STARKWARE_AIR_CPU_BUILTIN_MODULO_ADD_MOD_BUILTIN_PROVER_CONTEXT_H_

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/memory/memory.h"
#include "starkware/air/cpu/builtin/modulo/mod_builtin_prover_context.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT, size_t NWords>
class AddModBuiltinProverContext : public ModBuiltinProverContext<FieldElementT, NWords> {
 public:
  using ValueType = typename FieldElementT::ValueType;
  using BigInteger = typename ModBuiltinProverContext<FieldElementT, NWords>::BigInteger;
  using Input = typename ModBuiltinProverContext<FieldElementT, NWords>::Input;
  using BatchSlice = typename ModBuiltinProverContext<FieldElementT, NWords>::BatchSlice;

  AddModBuiltinProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>* memory_pool, const uint64_t begin_addr, size_t n_instances,
      size_t batch_size, size_t word_bit_len, std::map<uint64_t, Input> inputs)
      : ModBuiltinProverContext<FieldElementT, NWords>(
            name, ctx, memory_pool, begin_addr, n_instances, batch_size, word_bit_len,
            std::move(inputs)),
        sub_p_bit_(ctx.GetVirtualColumn(name + "/sub_p_bit")),
        carry_bit_(this->InitVirtualColumns(name + "/carry", "_bit", ctx, 1, NWords)),
        carry_sign_(this->InitVirtualColumns(name + "/carry", "_sign", ctx, 1, NWords)) {}

  /*
    Writes the trace cells for the builtin.
  */
  void WriteTrace(gsl::span<const gsl::span<FieldElementT>> trace) const;

 private:
  /*
    Used in WriteTrace to write the trace cell for one carry bit and sign.
    The output is the product of the carry bit and the sign, for calculation
    of next iteration's carry.
  */
  ValueType WriteCarry(
      gsl::span<const gsl::span<FieldElementT>> trace, const ValueType& carry,
      const size_t instance, const size_t index_1d,
      const std::function<std::string()>& args_str_gen) const;

  /*
    Virtual columns.
  */
  const VirtualColumn sub_p_bit_;
  const std::vector<VirtualColumn> carry_bit_;
  const std::vector<VirtualColumn> carry_sign_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/builtin/modulo/add_mod_builtin_prover_context.inl"

#endif  // STARKWARE_AIR_CPU_BUILTIN_MODULO_ADD_MOD_BUILTIN_PROVER_CONTEXT_H_
