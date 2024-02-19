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

template <typename FieldElementT>
void KeccakBuiltinProverContext<FieldElementT>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  for (size_t i = 0; i < n_component_instances_; ++i) {
    constexpr size_t kPaddingSize =
        FieldElementT::SizeInBytes() - KeccakComponent<FieldElementT>::kBytesInWord;
    // The current_witness is padded with kPaddingSize to simplify FieldElementT::ToBytes() usage.
    std::vector<std::byte> current_witness(
        KeccakComponent<FieldElementT>::kStateSizeInBytes * n_invocations_ + kPaddingSize,
        std::byte(0));
    size_t pos = 0;
    for (size_t idx_in_batch = 0; idx_in_batch < n_invocations_; ++idx_in_batch) {
      size_t invocation = i * n_invocations_ + idx_in_batch;
      const auto& input_itr = inputs_.find(invocation);
      if (input_itr == inputs_.end()) {
        pos += KeccakComponent<FieldElementT>::kStateSizeInBytes;
      } else {
        for (const auto& element : input_itr->second) {
          element.ToBytes(
              gsl::make_span(current_witness).subspan(pos, FieldElementT::SizeInBytes()), false);
          pos += KeccakComponent<FieldElementT>::kBytesInWord;
        }
      }
    }
    const auto io = keccak_component_.WriteTrace(
        gsl::make_span(current_witness)
            .subspan(0, KeccakComponent<FieldElementT>::kStateSizeInBytes * n_invocations_),
        i, trace);
    constexpr size_t kInputOutputLength = 2 * KeccakComponent<FieldElementT>::kStateSizeInWords;
    ASSERT_RELEASE(io.size() == n_invocations_ * kInputOutputLength, "");
    for (size_t idx_in_batch = 0; idx_in_batch < n_invocations_; ++idx_in_batch) {
      size_t invocation = i * n_invocations_ + idx_in_batch;
      for (size_t index = 0; index < kInputOutputLength; ++index) {
        const uint64_t mem_addr = begin_addr_ + index + kInputOutputLength * invocation;
        mem_input_output_.WriteTrace(
            index + kInputOutputLength * invocation, mem_addr,
            io[index + kInputOutputLength * idx_in_batch], trace);
      }
    }
  }
}

template <typename FieldElementT>
auto KeccakBuiltinProverContext<FieldElementT>::ParsePrivateInput(const JsonValue& private_input)
    -> std::map<uint64_t, Input> {
  std::map<uint64_t, Input> res;

  for (size_t i = 0; i < private_input.ArrayLength(); ++i) {
    const auto& input = private_input[i];
    res.emplace(
        input["index"].AsUint64(), Input{ValueType::FromString(input["input_s0"].AsString()),
                                         ValueType::FromString(input["input_s1"].AsString()),
                                         ValueType::FromString(input["input_s2"].AsString()),
                                         ValueType::FromString(input["input_s3"].AsString()),
                                         ValueType::FromString(input["input_s4"].AsString()),
                                         ValueType::FromString(input["input_s5"].AsString()),
                                         ValueType::FromString(input["input_s6"].AsString()),
                                         ValueType::FromString(input["input_s7"].AsString())});
  }

  return res;
}

}  // namespace cpu
}  // namespace starkware
