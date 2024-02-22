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

#ifndef STARKWARE_AIR_CPU_BUILTIN_HASH_HASH_BUILTIN_PROVER_CONTEXT_H_
#define STARKWARE_AIR_CPU_BUILTIN_HASH_HASH_BUILTIN_PROVER_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/hash/hash_component.h"
#include "starkware/air/components/hash/hash_factory.h"
#include "starkware/air/components/memory/memory.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
class HashBuiltinProverContext {
 public:
  struct HashInput {
    FieldElementT x;
    FieldElementT y;
  };

  HashBuiltinProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      const HashFactory<FieldElementT>& hash_factory, MemoryCell<FieldElementT>* memory_pool,
      const uint64_t begin_addr, uint64_t n_instances, size_t repetitions,
      std::map<uint64_t, HashInput> inputs)
      : begin_addr_(begin_addr),
        n_instances_(n_instances),
        repetitions_(repetitions),
        inputs_(std::move(inputs)),
        mem_input0_(memory_pool, name + "/input0", ctx),
        mem_input1_(memory_pool, name + "/input1", ctx),
        mem_output_(memory_pool, name + "/output", ctx) {
    for (size_t rep = 0; rep < repetitions; rep++) {
      hash_components_.push_back(
          hash_factory.CreateComponent(name + "/hash" + std::to_string(rep), ctx));
    }
  }

  /*
    Writes the trace cells for the hash builtin.
  */
  void WriteTrace(gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    Parses the private input for the hash builtin. private_input should be a list of objects of the
    form {"index": <index of instance>, "x": <first input>, "y": <second input>}.
  */
  static std::map<uint64_t, HashInput> ParsePrivateInput(const JsonValue& private_input);

 private:
  const uint64_t begin_addr_;
  const uint64_t n_instances_;
  const size_t repetitions_;
  const std::map<uint64_t, HashInput> inputs_;

  const MemoryCellView<FieldElementT> mem_input0_;
  const MemoryCellView<FieldElementT> mem_input1_;
  const MemoryCellView<FieldElementT> mem_output_;
  std::vector<std::unique_ptr<const HashComponent<FieldElementT>>> hash_components_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/builtin/hash/hash_builtin_prover_context.inl"

#endif  // STARKWARE_AIR_CPU_BUILTIN_HASH_HASH_BUILTIN_PROVER_CONTEXT_H_
