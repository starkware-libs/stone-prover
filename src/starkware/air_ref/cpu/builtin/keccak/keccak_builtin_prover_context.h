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

#ifndef STARKWARE_AIR_CPU_BUILTIN_KECCAK_KECCAK_BUILTIN_PROVER_CONTEXT_H_
#define STARKWARE_AIR_CPU_BUILTIN_KECCAK_KECCAK_BUILTIN_PROVER_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/diluted_check/diluted_check_cell.h"
#include "starkware/air/components/keccak/keccak.h"
#include "starkware/air/components/memory/memory.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
class KeccakBuiltinProverContext {
 public:
  using ValueType = typename FieldElementT::ValueType;
  using Input = std::array<ValueType, 8>;

  KeccakBuiltinProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>* memory_pool,
      diluted_check_cell::DilutedCheckCell<FieldElementT>* diluted_pool, const uint64_t begin_addr,
      uint64_t n_component_instances, size_t diluted_spacing, size_t n_invocations,
      std::map<uint64_t, Input> inputs)
      : begin_addr_(begin_addr),
        n_component_instances_(n_component_instances),
        diluted_spacing_(diluted_spacing),
        n_invocations_(n_invocations),
        inputs_(std::move(inputs)),
        mem_input_output_(memory_pool, name + "/input_output", ctx),
        diluted_pools_{diluted_pool, diluted_pool, diluted_pool, diluted_pool},
        keccak_component_(name + "/keccak", ctx, n_invocations, diluted_pools_, diluted_spacing) {}

  /*
    Writes the trace cells for the builtin.
  */
  void WriteTrace(gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    Parses the private input for the keccak builtin. private_input should be a list of
    objects of the form:
    {"index": <index of instance>, "input_s0": <first element>, ..., "input_s7": <last element>}.
  */
  static std::map<uint64_t, Input> ParsePrivateInput(const JsonValue& private_input);

 private:
  const uint64_t begin_addr_;
  const uint64_t n_component_instances_;
  const size_t diluted_spacing_;
  const size_t n_invocations_;

  const std::map<uint64_t, Input> inputs_;

  const MemoryCellView<FieldElementT> mem_input_output_;
  std::vector<diluted_check_cell::DilutedCheckCell<FieldElementT>*> diluted_pools_;
  const KeccakComponent<FieldElementT> keccak_component_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/builtin/keccak/keccak_builtin_prover_context.inl"

#endif  // STARKWARE_AIR_CPU_BUILTIN_KECCAK_KECCAK_BUILTIN_PROVER_CONTEXT_H_
