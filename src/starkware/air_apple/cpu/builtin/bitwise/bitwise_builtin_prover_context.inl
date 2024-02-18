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
std::vector<uint64_t> BitwiseBuiltinProverContext<FieldElementT>::GeneratePartition(
    uint64_t diluted_spacing, uint64_t diluted_n_bits, uint64_t total_n_bits) {
  // The shortest positive length of contiguous sequence of bits that can be covered using shifts
  // of the diluted form mask.
  const uint64_t skip = diluted_spacing * diluted_n_bits;
  std::vector<uint64_t> partition;
  for (uint64_t a = 0; a < total_n_bits; a += skip) {
    for (uint64_t b = 0; b < diluted_spacing; ++b) {
      if (a + b < total_n_bits) {
        partition.push_back(a + b);
      }
    }
  }
  return partition;
}

template <typename FieldElementT>
std::map<uint64_t, TableCheckCellView<FieldElementT>>
BitwiseBuiltinProverContext<FieldElementT>::GenerateTrimUnpacking(
    const std::string& name, const TraceGenerationContext& ctx,
    diluted_check_cell::DilutedCheckCell<FieldElementT>* diluted_pool, uint64_t diluted_spacing,
    uint64_t diluted_n_bits, uint64_t total_n_bits, const std::vector<uint64_t>& partition) {
  std::map<uint64_t, TableCheckCellView<FieldElementT>> diluted_trim;
  for (const uint64_t shift : partition) {
    if (shift + diluted_spacing * (diluted_n_bits - 1) + 1 > total_n_bits) {
      diluted_trim.emplace(
          shift, TableCheckCellView<FieldElementT>(
                     diluted_pool, (name + "/trim_unpacking").append(std::to_string(shift)), ctx));
    }
  }
  return diluted_trim;
}

template <typename FieldElementT>
void BitwiseBuiltinProverContext<FieldElementT>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  uint64_t mask = 0;
  for (uint64_t bit = 0; bit < diluted_n_bits_; ++bit) {
    mask |= Pow2(bit * diluted_spacing_);
  }

  for (uint64_t i = 0; i < n_instances_; ++i) {
    const auto& input_itr = inputs_.find(i);
    const Input& input = input_itr == inputs_.end() ? Input{0x0_Z, 0x0_Z} : input_itr->second;
    const uint64_t mem_addr = begin_addr_ + 5 * i;

    mem_x_.WriteTrace(i, mem_addr, FieldElementT::FromBigInt(input.x), trace);
    mem_y_.WriteTrace(i, mem_addr + 1, FieldElementT::FromBigInt(input.y), trace);
    mem_x_and_y_.WriteTrace(i, mem_addr + 2, FieldElementT::FromBigInt(input.x & input.y), trace);
    mem_x_xor_y_.WriteTrace(i, mem_addr + 3, FieldElementT::FromBigInt(input.x ^ input.y), trace);
    mem_x_or_y_.WriteTrace(i, mem_addr + 4, FieldElementT::FromBigInt(input.x | input.y), trace);

    for (uint64_t part = 0; part < partition_.size(); ++part) {
      const uint64_t shift = partition_[part];
      diluted_var_pool_.WriteTrace(
          part + partition_.size() * 4 * i, (input.x >> shift)[0] & mask, trace);
      diluted_var_pool_.WriteTrace(
          part + partition_.size() * (1 + 4 * i), (input.y >> shift)[0] & mask, trace);
      diluted_var_pool_.WriteTrace(
          part + partition_.size() * (2 + 4 * i), ((input.x & input.y) >> shift)[0] & mask, trace);
      diluted_var_pool_.WriteTrace(
          part + partition_.size() * (3 + 4 * i), ((input.x ^ input.y) >> shift)[0] & mask, trace);
    }
    for (const auto& [shift, cell_view] : diluted_cells_trim_unpacking_) {
      uint64_t diluted_value = ((input.x | input.y) >> shift)[0] & mask;
      const uint64_t deviation = shift + diluted_spacing_ * (diluted_n_bits_ - 1) + 1 - total_n_bits_;
      const uint64_t delta = DivCeil(deviation, diluted_spacing_) * diluted_spacing_;
      diluted_value <<= delta;
      cell_view.WriteTrace(i, diluted_value, trace);
    }
  }
}

template <typename FieldElementT>
auto BitwiseBuiltinProverContext<FieldElementT>::ParsePrivateInput(const JsonValue& private_input)
    -> std::map<uint64_t, Input> {
  std::map<uint64_t, Input> res;

  for (uint64_t i = 0; i < private_input.ArrayLength(); ++i) {
    const auto& input = private_input[i];
    res.emplace(
        input["index"].AsUint64(), Input{/*x=*/ValueType::FromString(input["x"].AsString()),
                                         /*y=*/ValueType::FromString(input["y"].AsString())});
  }

  return res;
}

}  // namespace cpu
}  // namespace starkware
