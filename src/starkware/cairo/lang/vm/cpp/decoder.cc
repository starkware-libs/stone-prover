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

#include "starkware/cairo/lang/vm/cpp/decoder.h"

namespace starkware {
namespace cpu {

namespace {

/*
  Takes a 'flags' arguments and three bits, the third of which is optional, and returns a three-bit
  number whose bits are the corresponding bits from 'flags'.
*/
uint64_t BitSelector(uint64_t flags, uint64_t bit0, uint64_t bit1, uint64_t bit2) {
  return (((flags >> bit2) & 1) << 2) | (((flags >> bit1) & 1) << 1) | ((flags >> bit0) & 1);
}

uint64_t BitSelector(uint64_t flags, uint64_t bit0, uint64_t bit1) {
  return (((flags >> bit1) & 1) << 1) | ((flags >> bit0) & 1);
}

}  // namespace

DecodedInstruction DecodedInstruction::DecodeInstruction(const uint64_t encoded_instruction) {
  const uint64_t off0 = encoded_instruction & kOffsetMask;
  const uint64_t off1 = (encoded_instruction >> kOffsetBits) & kOffsetMask;
  const uint64_t off2 = (encoded_instruction >> (2 * kOffsetBits)) & kOffsetMask;
  const uint64_t flags = encoded_instruction >> (3 * kOffsetBits);
  return {/*off0=*/off0, /*off1=*/off1, /*off2=*/off2, /*flags=*/flags};
}

// Decoder functions from flags to instruction attributes.

Op1Addr DecodeOp1Addr(const uint64_t flags) {
  std::array<Op1Addr, 5> values(
      {Op1Addr::kOp0, Op1Addr::kFp, Op1Addr::kAp, Op1Addr::kError, Op1Addr::kImm});
  return values.at(BitSelector(flags, kOp1FpBit, kOp1ApBit, kOp1ImmBit));
}

Res DecodeRes(const uint64_t flags) {
  std::array<Res, 4> values{Res::kOp1, Res::kMul, Res::kAdd, Res::kError};
  return values.at(BitSelector(flags, kResMulBit, kResAddBit));
}

PcUpdate DecodePcUpdate(const uint64_t flags) {
  std::array<PcUpdate, 5> values(
      {PcUpdate::kRegular, PcUpdate::kJnz, PcUpdate::kJumpRel, PcUpdate::kError, PcUpdate::kJump});
  return values.at(BitSelector(flags, kPcJnzBit, kPcJumpRelBit, kPcJumpAbsBit));
}

Opcode DecodeOpcode(const uint64_t flags) {
  std::array<Opcode, 5> values(
      {Opcode::kNop, Opcode::kAssertEq, Opcode::kRet, Opcode::kError, Opcode::kCall});
  return values.at(BitSelector(flags, kOpcodeAssertEqBit, kOpcodeRetBit, kOpcodeCallBit));
}

ApUpdate DecodeApUpdate(const uint64_t flags) {
  std::array<ApUpdate, 3> values({ApUpdate::kRegular, ApUpdate::kAdd1, ApUpdate::kAdd});
  return values.at(BitSelector(flags, kApAdd1Bit, kApAddBit));
}

Instruction::Instruction(const DecodedInstruction& decoded_instruction) {
  off0 = decoded_instruction.off0 - Pow2(kOffsetBits - 1);
  off1 = decoded_instruction.off1 - Pow2(kOffsetBits - 1);
  off2 = decoded_instruction.off2 - Pow2(kOffsetBits - 1);
  dst_register =
      (decoded_instruction.flags & (1 << kDstRegBit)) != 0 ? Register::kFp : Register::kAp;
  op0_register =
      (decoded_instruction.flags & (1 << kOp0RegBit)) != 0 ? Register::kFp : Register::kAp;
  op1_addr = DecodeOp1Addr(decoded_instruction.flags);
  res = DecodeRes(decoded_instruction.flags);
  pc_update = DecodePcUpdate(decoded_instruction.flags);
  if (pc_update == PcUpdate::kJnz) {
    res = Res::kUnconstrained;
  }
  ap_update = DecodeApUpdate(decoded_instruction.flags);
  opcode = DecodeOpcode(decoded_instruction.flags);
  if (opcode == Opcode::kCall) {
    ap_update = ApUpdate::kAdd2;
    fp_update = FpUpdate::kApPlus2;
  } else if (opcode == Opcode::kRet) {
    fp_update = FpUpdate::kDst;
  } else {
    fp_update = FpUpdate::kRegular;
  }
}

uint64_t Instruction::InstructionSize() const { return op1_addr == Op1Addr::kImm ? 2 : 1; }

}  // namespace cpu
}  // namespace starkware
