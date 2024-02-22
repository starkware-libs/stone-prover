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

template <typename FieldElementT>
void HashBuiltinProverContext<FieldElementT>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  const HashInput dummy_input{FieldElementT::Zero(), FieldElementT::Zero()};

  TaskManager::GetInstance().ParallelFor(n_instances_, [&](const TaskInfo& task_info) {
    const size_t idx = task_info.start_idx;
    const uint64_t mem_addr = begin_addr_ + 3 * idx;

    const auto& input_itr = inputs_.find(idx);
    const HashInput& input = input_itr != inputs_.end() ? input_itr->second : dummy_input;
    FieldElementT output = hash_components_[idx % repetitions_]->WriteTrace(
        std::array<FieldElementT, 2>{input.x, input.y}, idx / repetitions_, trace);

    mem_input0_.WriteTrace(idx, mem_addr, input.x, trace);
    mem_input1_.WriteTrace(idx, mem_addr + 1, input.y, trace);
    mem_output_.WriteTrace(idx, mem_addr + 2, output, trace);
  });
}

template <typename FieldElementT>
auto HashBuiltinProverContext<FieldElementT>::ParsePrivateInput(const JsonValue& private_input)
    -> std::map<uint64_t, HashInput> {
  std::map<uint64_t, HashInput> res;

  for (size_t i = 0; i < private_input.ArrayLength(); ++i) {
    const auto& input = private_input[i];
    res.emplace(
        input["index"].AsUint64(), HashInput{
                                       /*x=*/input["x"].AsFieldElement<FieldElementT>(),
                                       /*y=*/input["y"].AsFieldElement<FieldElementT>(),
                                   });
  }

  return res;
}

}  // namespace cpu
}  // namespace starkware
