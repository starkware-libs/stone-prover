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

#include "starkware/algebra/field_to_int.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
void CpuComponent<FieldElementT>::WriteTrace(
    const uint64_t instruction_index, const TraceEntry<FieldElementT>& values,
    const CpuMemory<FieldElementT>& memory, MemoryCell<FieldElementT>* memory_cell,
    RangeCheckCell<FieldElementT>* range_check_cell,
    const gsl::span<const gsl::span<FieldElementT>> trace) const {
  // Get prover context.
  ProverContext prover_ctx(name_, ctx_, memory_cell, range_check_cell);

  // "decode" columns.
  const uint64_t encoded_instruction = ToUint64(memory.At(values.pc));
  const auto decoded_inst = DecodedInstruction::DecodeInstruction(encoded_instruction);
  const Instruction inst(decoded_inst);

  prover_ctx.mem_pc.WriteTrace(
      instruction_index, values.pc, FieldElementT::FromUint(encoded_instruction), trace);
  prover_ctx.rc_off0.WriteTrace(instruction_index, decoded_inst.off0, trace);
  prover_ctx.rc_off1.WriteTrace(instruction_index, decoded_inst.off1, trace);
  prover_ctx.rc_off2.WriteTrace(instruction_index, decoded_inst.off2, trace);
  opcode_rc_.WriteTrace(decoded_inst.flags, instruction_index, trace);

  const uint64_t dst_addr = values.ComputeDstAddr(inst);
  const FieldElementT dst_value = memory.At(dst_addr);
  const uint64_t op0_addr = values.ComputeOp0Addr(inst);
  const FieldElementT op0_value = memory.At(op0_addr);
  const uint64_t op1_addr = values.ComputeOp1Addr(inst, op0_value);
  const FieldElementT op1_value = memory.At(op1_addr);

  // "operands" columns.
  prover_ctx.mem_dst.WriteTrace(instruction_index, dst_addr, dst_value, trace);
  prover_ctx.mem_op0.WriteTrace(instruction_index, op0_addr, op0_value, trace);
  prover_ctx.mem_op1.WriteTrace(instruction_index, op1_addr, op1_value, trace);

  const FieldElementT mul_value = op0_value * op1_value;
  mul_column_.SetCell(trace, instruction_index, mul_value);

  const bool res_add = ((decoded_inst.flags >> kResAddBit) & 1) != 0;
  const bool res_mul = ((decoded_inst.flags >> kResMulBit) & 1) != 0;
  const bool pc_jnz = ((decoded_inst.flags >> kPcJnzBit) & 1) != 0;
  const size_t total_res_flags =
      static_cast<size_t>(res_add) + static_cast<size_t>(res_mul) + static_cast<size_t>(pc_jnz);
  ASSERT_RELEASE(total_res_flags <= 1, "Invalid RES flags in instruction");
  const FieldElementT res_value =
      res_add ? op0_value + op1_value
              : (res_mul ? mul_value
                         : (pc_jnz ? (dst_value == FieldElementT::Zero() ? FieldElementT::Zero()
                                                                         : dst_value.Inverse())
                                   : op1_value));
  res_column_.SetCell(trace, instruction_index, res_value);

  // "registers" columns.
  ap_column_.SetCell(trace, instruction_index, values.ap);
  fp_column_.SetCell(trace, instruction_index, values.fp);

  // "update_registers" columns.
  const FieldElementT jnz_tmp0_value = pc_jnz ? dst_value : FieldElementT::Zero();
  jnz_tmp0_column_.SetCell(trace, instruction_index, jnz_tmp0_value);
  jnz_tmp1_column_.SetCell(trace, instruction_index, jnz_tmp0_value * res_value);
}

}  // namespace cpu
}  // namespace starkware
