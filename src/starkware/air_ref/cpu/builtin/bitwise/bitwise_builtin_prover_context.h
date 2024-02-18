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

#ifndef STARKWARE_AIR_CPU_BUILTIN_BITWISE_BITWISE_BUILTIN_PROVER_CONTEXT_H_
#define STARKWARE_AIR_CPU_BUILTIN_BITWISE_BITWISE_BUILTIN_PROVER_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/diluted_check/diluted_check_cell.h"
#include "starkware/air/components/memory/memory.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
class BitwiseBuiltinProverContext {
 public:
  using ValueType = typename FieldElementT::ValueType;

  struct Input {
    ValueType x;
    ValueType y;
  };

  BitwiseBuiltinProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>* memory_pool,
      diluted_check_cell::DilutedCheckCell<FieldElementT>* diluted_pool, const uint64_t begin_addr,
      uint64_t n_instances, size_t diluted_spacing, size_t diluted_n_bits, size_t total_n_bits,
      std::map<uint64_t, Input> inputs)
      : begin_addr_(begin_addr),
        n_instances_(n_instances),
        diluted_spacing_(diluted_spacing),
        diluted_n_bits_(diluted_n_bits),
        total_n_bits_(total_n_bits),
        inputs_(std::move(inputs)),
        mem_x_(memory_pool, name + "/x", ctx),
        mem_y_(memory_pool, name + "/y", ctx),
        mem_x_and_y_(memory_pool, name + "/x_and_y", ctx),
        mem_x_xor_y_(memory_pool, name + "/x_xor_y", ctx),
        mem_x_or_y_(memory_pool, name + "/x_or_y", ctx),
        diluted_var_pool_(diluted_pool, name + "/diluted_var_pool", ctx),
        partition_(GeneratePartition(diluted_spacing, diluted_n_bits, total_n_bits)),
        diluted_cells_trim_unpacking_(GenerateTrimUnpacking(
            name, ctx, diluted_pool, diluted_spacing, diluted_n_bits, total_n_bits, partition_)) {
    ASSERT_RELEASE(
        (diluted_n_bits_ - 1) * diluted_spacing_ < 64,
        "Mask size larger than 64 bits is not implemented.");
  }

  /*
    Writes the used diluted check values for the builtin.
    Call Finalize() after the diluted check cell was finalized.
  */
  void WriteTrace(gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    Parses the private input for the bitwise builtin. private_input should be a list of
    objects of the form {"index": <index of instance>, "x": <first input>, "y": <second input>}.
  */
  static std::map<uint64_t, Input> ParsePrivateInput(const JsonValue& private_input);

 private:
  /*
    Construct the partition_ vector.
  */
  static std::vector<size_t> GeneratePartition(
      size_t diluted_spacing, size_t diluted_n_bits, size_t total_n_bits);

  /*
    Constructs the diluted_cells_trim_unpacking_ mapping.
  */
  std::map<size_t, TableCheckCellView<FieldElementT>> GenerateTrimUnpacking(
      const std::string& name, const TraceGenerationContext& ctx,
      diluted_check_cell::DilutedCheckCell<FieldElementT>* diluted_pool, size_t diluted_spacing,
      size_t diluted_n_bits, size_t total_n_bits, const std::vector<size_t>& partition);

  const uint64_t begin_addr_;
  const uint64_t n_instances_;
  const size_t diluted_spacing_;
  const size_t diluted_n_bits_;
  const size_t total_n_bits_;

  const std::map<uint64_t, Input> inputs_;

  const MemoryCellView<FieldElementT> mem_x_;
  const MemoryCellView<FieldElementT> mem_y_;
  const MemoryCellView<FieldElementT> mem_x_and_y_;
  const MemoryCellView<FieldElementT> mem_x_xor_y_;
  const MemoryCellView<FieldElementT> mem_x_or_y_;

  const TableCheckCellView<FieldElementT> diluted_var_pool_;

  /*
    The partition of a total_n_bits bits register into shifted diluted parts.
    For example, if diluted_spacing=2, diluted_n_bits=3 and total_n_bits=7 then the partition of a
    7-bits register is as follows:

        bits      shift
        00*0*0*     0
        0*0*0*0     1
    *0*0*000000     6

    So the values of partition_ in this case are [0, 1, 6].
    Note that some of the shifted diluted parts may deviate and should be trimmed.
  */
  const std::vector<size_t> partition_;

  /*
    Maps a deviating element of partition_ to the diluted cell that trims it.
  */
  const std::map<size_t, TableCheckCellView<FieldElementT>> diluted_cells_trim_unpacking_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/builtin/bitwise/bitwise_builtin_prover_context.inl"

#endif  // STARKWARE_AIR_CPU_BUILTIN_BITWISE_BITWISE_BUILTIN_PROVER_CONTEXT_H_
