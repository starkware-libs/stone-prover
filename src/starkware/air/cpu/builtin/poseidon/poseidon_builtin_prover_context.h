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

#ifndef STARKWARE_AIR_CPU_BUILTIN_POSEIDON_POSEIDON_BUILTIN_PROVER_CONTEXT_H_
#define STARKWARE_AIR_CPU_BUILTIN_POSEIDON_POSEIDON_BUILTIN_PROVER_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/memory/memory.h"
#include "starkware/air/components/poseidon/poseidon.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT, uint64_t M>
class PoseidonBuiltinProverContext {
 public:
  using Input = std::array<FieldElementT, M>;

  PoseidonBuiltinProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>* memory_pool, const uint64_t begin_addr,
      uint64_t n_component_instances, std::map<uint64_t, Input> inputs, uint64_t rounds_full,
      uint64_t rounds_partial, gsl::span<const uint64_t> partial_rounds_partition,
      const ConstSpanAdapter<FieldElementT>& mds, const ConstSpanAdapter<FieldElementT>& ark)
      : begin_addr_(begin_addr),
        n_component_instances_(n_component_instances),
        inputs_(std::move(inputs)),
        mem_input_output_(InitMemInputOutput(name, ctx, memory_pool)),
        poseidon_component_(
            name + "/poseidon", ctx, M, rounds_full, rounds_partial, partial_rounds_partition, mds,
            ark) {}

  /*
    Writes the trace cells for the builtin.
  */
  void WriteTrace(gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    Parses the private input for the poseidon builtin. private_input should be a list of
    objects of the form:
    {"index": <index of instance>, "input_s0": <first element>, "input_s1": <second element>,
    ...}.
  */
  static std::map<uint64_t, Input> ParsePrivateInput(const JsonValue& private_input);

 private:
  static constexpr Input ZeroInput() {
    Input res = UninitializedFieldElementArray<FieldElementT, M>();
    for (uint64_t i = 0; i < M; ++i) {
      res[i] = FieldElementT::Zero();
    }
    return res;
  }

  static constexpr std::vector<MemoryCellView<FieldElementT>> InitMemInputOutput(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>* memory_pool) {
    std::vector<MemoryCellView<FieldElementT>> res;
    res.reserve(M);
    for (uint64_t i = 0; i < M; ++i) {
      res.push_back(MemoryCellView<FieldElementT>(
          memory_pool, name + "/param_" + std::to_string(i) + "/input_output", ctx));
    }
    return res;
  }

  const uint64_t begin_addr_;
  const uint64_t n_component_instances_;

  const std::map<uint64_t, Input> inputs_;

  const std::vector<MemoryCellView<FieldElementT>> mem_input_output_;
  const PoseidonComponent<FieldElementT> poseidon_component_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/builtin/poseidon/poseidon_builtin_prover_context.inl"

#endif  // STARKWARE_AIR_CPU_BUILTIN_POSEIDON_POSEIDON_BUILTIN_PROVER_CONTEXT_H_
