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

#include <sstream>
#include <string>

#include "gtest/gtest.h"

#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/cairo/lang/vm/cpp/trace_utils.h"

namespace starkware {
namespace cpu {
namespace {

using FieldElementT = PrimeFieldElement<252, 0>;

TEST(CpuMemory, ReadFile) {
  std::istringstream is(std::string(
      "\x01\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00"
      "\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      80));
  CpuMemory<FieldElementT> memory = CpuMemory<FieldElementT>::ReadFile(&is);
  EXPECT_EQ(memory.At(1), FieldElementT::FromUint(2));
  EXPECT_EQ(memory.At(3), FieldElementT::FromUint(4));
  EXPECT_EQ(memory.Size(), 2);
}

}  // namespace
}  // namespace cpu
}  // namespace starkware
