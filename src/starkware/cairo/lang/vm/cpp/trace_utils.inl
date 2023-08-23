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
CpuMemory<FieldElementT> CpuMemory<FieldElementT>::ReadFile(std::istream* file) {
  using FieldValueType = typename FieldElementT::ValueType;
  std::map<uint64_t, FieldElementT> memory;
  uint64_t address;
  FieldElementT value = FieldElementT::Uninitialized();
  std::array<char, sizeof(uint64_t) + FieldElementT::SizeInBytes()> buffer{};
  const auto buffer_span = gsl::make_span(buffer);
  while (true) {
    file->read(buffer_span.data(), buffer_span.size());
    if (file->eof()) {
      if (file->gcount() > 0) {
        ASSERT_RELEASE(
            static_cast<bool>(*file), "Unexpected end of file. Read " +
                                          std::to_string(file->gcount()) + " out of " +
                                          std::to_string(buffer_span.size()));
      }
      break;
    }
    address = buffer_span.subspan(0, sizeof(uint64_t)).template as_span<const uint64_t>().at(0);
    value = FieldElementT::FromBigInt(::starkware::Deserialize<FieldValueType>(
        buffer_span.subspan(sizeof(uint64_t), FieldElementT::SizeInBytes())
            .template as_span<std::byte>(),
        /*use_big_endian=*/false));
    ASSERT_RELEASE(static_cast<bool>(*file), "Error reading memory from the file.");
    memory.emplace(address, value);
  }
  return CpuMemory(std::move(memory));
}

template <typename FieldElementT>
TraceEntry<FieldElementT> TraceEntry<FieldElementT>::Deserialize(
    const gsl::span<const std::byte> input) {
  ASSERT_RELEASE(input.size() == kSerializationSize, "Wrong input size.");
  // Create a span for the uint64_t values.
  const auto uint64_span =
      input.first(kNUint64Elements * sizeof(uint64_t)).template as_span<const uint64_t>();

  return TraceEntry<FieldElementT>{
      /*ap=*/FieldElementT::FromUint(uint64_span.at(0)),
      /*fp=*/FieldElementT::FromUint(uint64_span.at(1)),
      /*pc=*/uint64_span.at(2),
  };
}

template <typename FieldElementT>
std::vector<TraceEntry<FieldElementT>> TraceEntry<FieldElementT>::ReadFile(std::istream* file) {
  std::vector<TraceEntry> trace;
  std::array<std::byte, kSerializationSize> buffer{};
  const auto buffer_span = gsl::make_span(buffer).template as_span<char>();
  while (true) {
    file->read(buffer_span.data(), buffer_span.size());
    if (file->eof()) {
      if (file->gcount() > 0) {
        ASSERT_RELEASE(
            static_cast<bool>(*file), "Unexpected end of file. Read " +
                                          std::to_string(file->gcount()) + " out of " +
                                          std::to_string(buffer_span.size()));
      }
      break;
    }
    ASSERT_RELEASE(static_cast<bool>(*file), "Error reading TraceEntry from the file.");
    trace.push_back(Deserialize(buffer));
  }
  return trace;
}

template <typename FieldElementT>
uint64_t TraceEntry<FieldElementT>::ComputeDstAddr(const Instruction& instruction) const {
  FieldElementT base_addr = FieldElementT::Uninitialized();
  if (instruction.dst_register == Register::kAp) {
    base_addr = ap;
  } else if (instruction.dst_register == Register::kFp) {
    base_addr = fp;
  } else {
    THROW_STARKWARE_EXCEPTION("Unknown value for dst_register.");
  }
  return ToUint64(base_addr + FieldElementT::FromInt(instruction.off0));
}

template <typename FieldElementT>
uint64_t TraceEntry<FieldElementT>::ComputeOp0Addr(const Instruction& instruction) const {
  FieldElementT base_addr = FieldElementT::Uninitialized();
  if (instruction.op0_register == Register::kAp) {
    base_addr = ap;
  } else if (instruction.op0_register == Register::kFp) {
    base_addr = fp;
  } else {
    THROW_STARKWARE_EXCEPTION("Unknown value for op0_register.");
  }
  return ToUint64(base_addr + FieldElementT::FromInt(instruction.off1));
}

template <typename FieldElementT>
uint64_t TraceEntry<FieldElementT>::ComputeOp1Addr(
    const Instruction& instruction, const FieldElementT& op0) const {
  FieldElementT base_addr = FieldElementT::Uninitialized();
  if (instruction.op1_addr == Op1Addr::kFp) {
    base_addr = fp;
  } else if (instruction.op1_addr == Op1Addr::kAp) {
    base_addr = ap;
  } else if (instruction.op1_addr == Op1Addr::kImm) {
    base_addr = FieldElementT::FromUint(pc);
  } else if (instruction.op1_addr == Op1Addr::kOp0) {
    base_addr = op0;
  } else {
    THROW_STARKWARE_EXCEPTION("Unknown value for op1_register.");
  }

  return ToUint64(base_addr + FieldElementT::FromInt(instruction.off2));
}

}  // namespace cpu
}  // namespace starkware
