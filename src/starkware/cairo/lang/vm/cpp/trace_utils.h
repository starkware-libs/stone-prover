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

#ifndef STARKWARE_CAIRO_LANG_VM_CPP_TRACE_UTILS_H_
#define STARKWARE_CAIRO_LANG_VM_CPP_TRACE_UTILS_H_

#include <map>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/cairo/lang/vm/cpp/decoder.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/serialization.h"

namespace starkware {
namespace cpu {

/*
  Represents the CPU memory.
*/
template <typename FieldElementT>
class CpuMemory {
 public:
  explicit CpuMemory(std::map<uint64_t, FieldElementT> memory) : memory_(std::move(memory)) {}
  /*
    Reads a memory file structured as pairs of an 8 byte little endian address followed by a
    little endian value of FieldElementT and sets the internal map 'memory' accordingly.
  */
  static CpuMemory<FieldElementT> ReadFile(std::istream* file);

  /*
    Exposed implementations of the internal map's 'at' and 'size'.
  */
  FieldElementT At(uint64_t addr) const {
    auto search = memory_.find(addr);
    ASSERT_RELEASE(search != memory_.end(), "Address not found in memory: " + std::to_string(addr));
    return search->second;
  }
  size_t Size() const { return memory_.size(); }

 private:
  std::map<uint64_t, FieldElementT> memory_;
};

/*
  Represents the values of the trace for the execution of an instruction.
*/
template <typename FieldElementT>
struct TraceEntry {
  FieldElementT ap;
  FieldElementT fp;

  uint64_t pc;

  using FieldValueType = typename FieldElementT::ValueType;
  static constexpr size_t kNUint64Elements = 3;
  static constexpr size_t kSerializationSize = kNUint64Elements * sizeof(uint64_t);

  /*
    Reads a TraceEntry from the file.
    See TraceEntry.serialize() (src/starkware/cairo/lang/vm/vm.py) in python for the
    serialization format.
  */
  static TraceEntry Deserialize(gsl::span<const std::byte> input);

  /*
    Reads all the trace entries in the file.
  */
  static std::vector<TraceEntry> ReadFile(std::istream* file);

  /*
    Returns the address of the dst operand in the given instruction.
  */
  uint64_t ComputeDstAddr(const Instruction& instruction) const;

  /*
    Returns the address of the op0 operand in the given instruction.
  */
  uint64_t ComputeOp0Addr(const Instruction& instruction) const;

  /*
    Returns the address of the op1 operand in the given instruction.
  */
  uint64_t ComputeOp1Addr(const Instruction& instruction, const FieldElementT& op0) const;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/cairo/lang/vm/cpp/trace_utils.inl"

#endif  // STARKWARE_CAIRO_LANG_VM_CPP_TRACE_UTILS_H_
