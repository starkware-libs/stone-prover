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
auto ModBuiltinProverContext<FieldElementT, NWords>::ParsePrivateInput(
    const JsonValue& private_input, const size_t batch_size) -> std::map<uint64_t, Input> {
  std::map<uint64_t, Input> res;
  auto instances = private_input["instances"];
  auto private_input_len = instances.ArrayLength();
  for (size_t inst = 0; inst < private_input_len; ++inst) {
    const auto& input = instances[inst];
    const auto& input_batch = input["batch"];
    std::vector<BatchSlice> curr_batch;
    ASSERT_RELEASE(
        input_batch.ArrayLength() == batch_size,
        "Invalid input: batch sizes should all be " + std::to_string(batch_size) + ".");
    curr_batch.reserve(batch_size);
    for (size_t ind = 0; ind < batch_size; ++ind) {
      curr_batch.push_back(BatchSlice{
          input_batch[ind]["a_offset"].AsUint64(),
          input_batch[ind]["b_offset"].AsUint64(),
          input_batch[ind]["c_offset"].AsUint64(),
          ParseBigInts(input_batch[ind], "a"),
          ParseBigInts(input_batch[ind], "b"),
          ParseBigInts(input_batch[ind], "c"),
      });
    }

    res.emplace(
        input["index"].AsUint64(), Input{
                                       /*p=*/ParseBigInts(input, "p"),
                                       /*values_ptr=*/input["values_ptr"].AsUint64(),
                                       /*offsets_ptr=*/input["offsets_ptr"].AsUint64(),
                                       /*n=*/input["n"].AsUint64(),
                                       /*batch=*/curr_batch});
  }
  uint64_t zero_value_begin_addr = private_input["zero_value_address"].AsUint64();
  const Input zero_input = Input(
      /*p=*/std::vector<ValueType>(NWords, ValueType::Zero()),
      /*values_ptr=*/zero_value_begin_addr,
      /*offsets_ptr=*/zero_value_begin_addr,
      /*n=*/batch_size,
      /*batch=*/
      std::vector<BatchSlice>(
          batch_size, ModBuiltinProverContext<FieldElementT, NWords>::zero_batch_slice_));
  res.emplace(private_input_len, zero_input);
  return res;
}

template <typename FieldElementT, size_t NWords>
const typename ModBuiltinProverContext<FieldElementT, NWords>::Input&
ModBuiltinProverContext<FieldElementT, NWords>::WriteInput(
    gsl::span<const gsl::span<FieldElementT>> trace, const size_t instance) const {
  const auto& input_itr = inputs_.find(instance);
  const auto& zero_input = inputs_.rbegin()->second;
  const Input& input = input_itr == inputs_.end() ? zero_input : input_itr->second;
  const uint64_t mem_addr = begin_addr_ + (NWords + 3) * instance;

  for (size_t word = 0; word < NWords; ++word) {
    mem_p_[word].WriteTrace(
        instance, mem_addr + word, FieldElementT::FromBigInt(input.p[word]), trace);
  }
  mem_values_ptr_.WriteTrace(
      instance, mem_addr + NWords, FieldElementT::FromUint(input.values_ptr), trace);
  mem_offsets_ptr_.WriteTrace(
      instance, mem_addr + NWords + 1, FieldElementT::FromUint(input.offsets_ptr), trace);
  mem_n_.WriteTrace(instance, mem_addr + NWords + 2, FieldElementT::FromUint(input.n), trace);
  for (size_t ind = 0; ind < batch_size_; ++ind) {
    const size_t index_1d = instance * batch_size_ + ind;
    mem_a_offset_.WriteTrace(
        index_1d, input.offsets_ptr + 3 * ind, FieldElementT::FromUint(input.batch[ind].a_offset),
        trace);
    mem_b_offset_.WriteTrace(
        index_1d, input.offsets_ptr + 3 * ind + 1,
        FieldElementT::FromUint(input.batch[ind].b_offset), trace);
    mem_c_offset_.WriteTrace(
        index_1d, input.offsets_ptr + 3 * ind + 2,
        FieldElementT::FromUint(input.batch[ind].c_offset), trace);
    for (size_t word = 0; word < NWords; ++word) {
      mem_a_[word].WriteTrace(
          index_1d, input.values_ptr + input.batch[ind].a_offset + word,
          FieldElementT::FromBigInt(input.batch[ind].a[word]), trace);
      mem_b_[word].WriteTrace(
          index_1d, input.values_ptr + input.batch[ind].b_offset + word,
          FieldElementT::FromBigInt(input.batch[ind].b[word]), trace);
      mem_c_[word].WriteTrace(
          index_1d, input.values_ptr + input.batch[ind].c_offset + word,
          FieldElementT::FromBigInt(input.batch[ind].c[word]), trace);
    }
  }
  return input;
}

}  // namespace cpu
}  // namespace starkware
