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

#ifndef STARKWARE_AIR_CPU_COMPONENT_CPU_COMPONENT_H_
#define STARKWARE_AIR_CPU_COMPONENT_CPU_COMPONENT_H_

#include <cstddef>
#include <istream>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/memory/memory.h"
#include "starkware/air/components/perm_range_check/range_check_cell.h"
#include "starkware/air/components/range_check/range_check.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/cairo/lang/vm/cpp/decoder.h"
#include "starkware/cairo/lang/vm/cpp/trace_utils.h"
#include "starkware/math/math.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
class CpuComponent {
 public:
  class ProverContext {
   public:
    ProverContext(
        const std::string& name, const TraceGenerationContext& ctx,
        MemoryCell<FieldElementT>* memory_cell, RangeCheckCell<FieldElementT>* rc_cell)
        : mem_pc(memory_cell, name + "/decode/mem_inst", ctx),
          mem_dst(memory_cell, name + "/operands/mem_dst", ctx),
          mem_op0(memory_cell, name + "/operands/mem_op0", ctx),
          mem_op1(memory_cell, name + "/operands/mem_op1", ctx),
          rc_off0(rc_cell, name + "/decode/off0", ctx),
          rc_off1(rc_cell, name + "/decode/off1", ctx),
          rc_off2(rc_cell, name + "/decode/off2", ctx) {}

    // Memory cell views.
    MemoryCellView<FieldElementT> mem_pc;
    MemoryCellView<FieldElementT> mem_dst;
    MemoryCellView<FieldElementT> mem_op0;
    MemoryCellView<FieldElementT> mem_op1;

    // Offset range check cell views.
    TableCheckCellView<FieldElementT> rc_off0;
    TableCheckCellView<FieldElementT> rc_off1;
    TableCheckCellView<FieldElementT> rc_off2;
  };

  CpuComponent(const std::string& name, const TraceGenerationContext& ctx)
      : name_(name),
        ctx_(ctx),
        opcode_rc_(name + "/decode/opcode_rc", ctx, kOffsetBits),
        mul_column_(ctx.GetVirtualColumn(name + "/operands/ops_mul")),
        res_column_(ctx.GetVirtualColumn(name + "/operands/res")),
        ap_column_(ctx.GetVirtualColumn(name + "/registers/ap")),
        fp_column_(ctx.GetVirtualColumn(name + "/registers/fp")),
        jnz_tmp0_column_(ctx.GetVirtualColumn(name + "/update_registers/update_pc/tmp0")),
        jnz_tmp1_column_(ctx.GetVirtualColumn(name + "/update_registers/update_pc/tmp1")) {}

  /*
    Writes the trace for one instruction of the component.
  */
  void WriteTrace(
      uint64_t instruction_index, const TraceEntry<FieldElementT>& values,
      const CpuMemory<FieldElementT>& memory, MemoryCell<FieldElementT>* memory_cell,
      RangeCheckCell<FieldElementT>* range_check_cell,
      gsl::span<const gsl::span<FieldElementT>> trace) const;

  static constexpr size_t kOffsetBits = 16;

 private:
  // Component name.
  const std::string name_;
  const TraceGenerationContext ctx_;

  // Flag locations.
  static constexpr size_t kResAddBit = 5;
  static constexpr size_t kResMulBit = 6;
  static constexpr size_t kPcJnzBit = 9;

  // "decode" columns.
  const RangeCheckComponent<FieldElementT> opcode_rc_;

  // "operands" columns.
  const VirtualColumn mul_column_;
  const VirtualColumn res_column_;

  // "registers" columns.
  const VirtualColumn ap_column_;
  const VirtualColumn fp_column_;

  // "update_registers" columns.
  const VirtualColumn jnz_tmp0_column_;
  const VirtualColumn jnz_tmp1_column_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/component/cpu_component.inl"

#endif  // STARKWARE_AIR_CPU_COMPONENT_CPU_COMPONENT_H_
