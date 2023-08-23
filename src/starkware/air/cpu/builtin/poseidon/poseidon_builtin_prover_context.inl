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

template <typename FieldElementT, size_t M>
void PoseidonBuiltinProverContext<FieldElementT, M>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  for (size_t i = 0; i < n_component_instances_; ++i) {
    const auto& input_itr = inputs_.find(i);
    const Input& input = input_itr == inputs_.end() ? ZeroInput() : input_itr->second;

    const uint64_t mem_addr = begin_addr_ + 2 * M * i;
    for (size_t index = 0; index < M; ++index) {
      mem_input_output_.WriteTrace(
          index + 2 * kMCapacity * i, mem_addr + index, input[index], trace);
    }

    const auto output = poseidon_component_.WriteTrace(input, i, trace);
    ASSERT_RELEASE(output.size() == M, "Wrong output length");

    for (size_t index = 0; index < M; ++index) {
      mem_input_output_.WriteTrace(
          M + index + 2 * kMCapacity * i, mem_addr + M + index, output[index], trace);
    }
  }
}

template <typename FieldElementT, size_t M>
auto PoseidonBuiltinProverContext<FieldElementT, M>::ParsePrivateInput(
    const JsonValue& private_input) -> std::map<uint64_t, Input> {
  std::map<uint64_t, Input> res;

  for (size_t i = 0; i < private_input.ArrayLength(); ++i) {
    const auto& input = private_input[i];
    Input arr = UninitializedFieldElementArray<FieldElementT, M>();
    for (size_t j = 0; j < M; ++j) {
      arr[j] = input["input_s" + std::to_string(j)].AsFieldElement<FieldElementT>();
    }
    res.emplace(input["index"].AsUint64(), arr);
  }

  return res;
}

}  // namespace cpu
}  // namespace starkware
