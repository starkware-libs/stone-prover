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

#ifndef STARKWARE_CAIRO_LANG_VM_CPP_DECODER_H_
#define STARKWARE_CAIRO_LANG_VM_CPP_DECODER_H_

#include <array>
#include <cstdint>
#include <optional>

#include "starkware/math/math.h"

namespace starkware {
namespace cpu {

const uint64_t kDstRegBit = 0;
const uint64_t kOp0RegBit = 1;
const uint64_t kOp1ImmBit = 2;
const uint64_t kOp1FpBit = 3;
const uint64_t kOp1ApBit = 4;
const uint64_t kResAddBit = 5;
const uint64_t kResMulBit = 6;
const uint64_t kPcJumpAbsBit = 7;
const uint64_t kPcJumpRelBit = 8;
const uint64_t kPcJnzBit = 9;
const uint64_t kApAddBit = 10;
const uint64_t kApAdd1Bit = 11;
const uint64_t kOpcodeCallBit = 12;
const uint64_t kOpcodeRetBit = 13;
const uint64_t kOpcodeAssertEqBit = 14;
// const uint64_t kReservedBit = 15;

const uint64_t kOffsetBits = 16;

struct DecodedInstruction {
  static constexpr uint64_t kOffsetMask = Pow2(16) - 1;

  /*
    Constructs an instance of DecodedInstruction from the given encoded instruction.
  */
  static DecodedInstruction DecodeInstruction(uint64_t encoded_instruction);

  uint64_t off0;
  uint64_t off1;
  uint64_t off2;
  uint64_t flags;
};

// Instruction components.
enum class Register { kAp, kFp, kError };
enum class Op1Addr { kImm, kAp, kFp, kOp0, kError };
enum class Res { kOp1, kAdd, kMul, kUnconstrained, kError };
enum class PcUpdate { kRegular, kJump, kJumpRel, kJnz, kError };
enum class ApUpdate { kRegular, kAdd, kAdd1, kAdd2, kError };
enum class FpUpdate { kRegular, kApPlus2, kDst, kError };
enum class Opcode { kNop, kAssertEq, kCall, kRet, kError };

struct Instruction {
  int64_t off0;
  int64_t off1;
  int64_t off2;
  Register dst_register;
  Register op0_register;
  Op1Addr op1_addr;
  Res res;
  PcUpdate pc_update;
  ApUpdate ap_update;
  FpUpdate fp_update;
  Opcode opcode;

  explicit Instruction(const DecodedInstruction& decoded_instruction);
  explicit Instruction(
      int64_t off0, int64_t off1, int64_t off2, Register dst_register, Register op0_register,
      Op1Addr op1_addr, Res res, PcUpdate pc_update, ApUpdate ap_update, FpUpdate fp_update,
      Opcode opcode)
      : off0(off0),
        off1(off1),
        off2(off2),
        dst_register(dst_register),
        op0_register(op0_register),
        op1_addr(op1_addr),
        res(res),
        pc_update(pc_update),
        ap_update(ap_update),
        fp_update(fp_update),
        opcode(opcode) {}

  uint64_t InstructionSize() const;
};

Op1Addr DecodeOp1Addr(uint64_t flags);
Res DecodeRes(uint64_t flags);
PcUpdate DecodePcUpdate(uint64_t flags);
Opcode DecodeOpcode(uint64_t flags);
ApUpdate DecodeApUpdate(uint64_t flags);

}  // namespace cpu
}  // namespace starkware

#endif  // STARKWARE_CAIRO_LANG_VM_CPP_DECODER_H_
