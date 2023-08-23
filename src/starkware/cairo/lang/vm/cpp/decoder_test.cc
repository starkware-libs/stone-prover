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

#include "gtest/gtest.h"

#include "starkware/algebra/fields/prime_field_element.h"

namespace starkware {
namespace cpu {

using FieldElementT = PrimeFieldElement<252, 0>;

TEST(Instruction, Instruction) {
  // Test decoding of "[ap] = 12345, ap++;"
  DecodedInstruction dec_inst = {32768, 32767, 32768, 51218};
  Instruction inst(dec_inst);
  ASSERT_EQ(inst.off0, 0);
  ASSERT_EQ(inst.off1, -1);
  ASSERT_EQ(inst.off2, 0);
  ASSERT_EQ(inst.dst_register, Register::kAp);
  ASSERT_EQ(inst.op0_register, Register::kFp);
  ASSERT_EQ(inst.op1_addr, Op1Addr::kAp);
  ASSERT_EQ(inst.res, Res::kOp1);
  ASSERT_EQ(inst.pc_update, PcUpdate::kRegular);
  ASSERT_EQ(inst.ap_update, ApUpdate::kAdd1);
  ASSERT_EQ(inst.fp_update, FpUpdate::kRegular);
  ASSERT_EQ(inst.opcode, Opcode::kAssertEq);
}

}  // namespace cpu
}  // namespace starkware
