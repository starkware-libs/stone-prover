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

#ifndef STARKWARE_AIR_CPU_BUILTIN_RANGE_CHECK_RANGE_CHECK_BUILTIN_PROVER_CONTEXT_H_
#define STARKWARE_AIR_CPU_BUILTIN_RANGE_CHECK_RANGE_CHECK_BUILTIN_PROVER_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "starkware/air/components/memory/memory.h"
#include "starkware/air/components/perm_range_check/range_check_cell.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
class RangeCheckBuiltinProverContext {
  using ValueType = typename FieldElementT::ValueType;

 public:
  RangeCheckBuiltinProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>* memory_pool, RangeCheckCell<FieldElementT>* rc_pool,
      const uint64_t begin_addr, uint64_t n_instances, const uint64_t n_parts,
      const size_t shift_bits, std::map<uint64_t, ValueType> inputs)
      : begin_addr_(begin_addr),
        n_instances_(n_instances),
        n_parts_(n_parts),
        shift_bits_(shift_bits),
        inputs_(std::move(inputs)),
        mem_input_(memory_pool, name + "/mem", ctx),
        rc_value_(rc_pool, name + "/inner_rc", ctx) {
    ASSERT_RELEASE(shift_bits_ < 64, "Range check builtin's shift must be at most 64.");
  }

  /*
    Writes the used range check values for the builtin.
    The unused values are not written so that they will be used to fill holes.
    The memory cells of the builtin are not written by this function.
    Call Finalize() after the range check cell was finalized.
  */
  void WriteTrace(gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    Writes the memory cells after finalization of range check cells.
  */
  void Finalize(gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    Parses the private input for the range check builtin. private_input should be a list of objects
    of the form {"index": <index of instance>, "value": <input>}.
  */
  static std::map<uint64_t, ValueType> ParsePrivateInput(const JsonValue& private_input);

 private:
  const uint64_t begin_addr_;
  const uint64_t n_instances_;
  const uint64_t n_parts_;
  const size_t shift_bits_;
  const std::map<uint64_t, ValueType> inputs_;

  const MemoryCellView<FieldElementT> mem_input_;
  const TableCheckCellView<FieldElementT> rc_value_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/builtin/range_check/range_check_builtin_prover_context.inl"

#endif  // STARKWARE_AIR_CPU_BUILTIN_RANGE_CHECK_RANGE_CHECK_BUILTIN_PROVER_CONTEXT_H_
