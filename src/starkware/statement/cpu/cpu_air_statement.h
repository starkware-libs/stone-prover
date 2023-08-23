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

#ifndef STARKWARE_STATEMENT_CPU_CPU_AIR_STATEMENT_H_
#define STARKWARE_STATEMENT_CPU_CPU_AIR_STATEMENT_H_

#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/cpu/board/cpu_air.h"
#include "starkware/air/cpu/board/cpu_air_trace_context.h"
#include "starkware/air/trace.h"
#include "starkware/statement/statement.h"
#include "starkware/utils/json.h"

namespace starkware {
namespace cpu {

class CpuAirStatement : public Statement {
 public:
  using FieldElementT = PrimeFieldElement<252, 0>;

  explicit CpuAirStatement(
      const JsonValue& statement_parameters, const JsonValue& public_input,
      std::optional<JsonValue> private_input);

  const Air& GetAir() override;

  /*
    Returns the initial hash chain seed which is the public input serialized as follows:
      n_steps          32 bytes
      rc_min           32 bytes
      rc_max           32 bytes
      layout           32 bytes
      For each memory segment:
        begin_addr     32 bytes
        stop_ptr       32 bytes
      Serialization of the public memory (see SerializePublicMemory()).

    Values are serialized using big endian.
  */
  const std::vector<std::byte> GetInitialHashChainSeed() const override;

  std::unique_ptr<TraceContext> GetTraceContext() const override;

  /*
    Same as GetTraceContext(), except that the trace file is given as an argument, instead of using
    the "trace_path" value in the private input json.
  */
  std::unique_ptr<TraceContext> GetTraceContextFromTraceFile(
      std::istream* trace_file, std::istream* memory_file) const;

  JsonValue FixPublicInput() override;

  std::string GetName() const override {
    std::string file_name = __FILE__;
    return ConvertFileNameToProverName(file_name);
  }

  uint64_t NSteps() const { return n_steps_; }
  const MemSegmentAddresses& GetMemSegmentAddresses() const { return mem_segment_addresses_; }
  const std::vector<MemoryAccessUnitData<FieldElementT>>& GetPublicMemory() {
    return public_memory_;
  }

  /*
    Disables some asserts in CpuAir. Should only be used in tests.
  */
  void DisableAssertsForTest();

 private:
  /*
    Adds the public memory page information to serializer.
    page_sizes should be a map from page id to page size.

    The serialization is as follows:
    * Number of pages.
    * Page 0: size, Hash of (addr0, M[addr0], addr1, M[addr1], ...).
    * Page 1: addr, size, Hash of (M[addr], M[addr + 1], ...).
    * Page 2, 3, ... are in the same format of page 1.
    * Address and value of the padding cell, which is used to pad the public memory to a power of 2.
  */
  void SerializePublicMemory(
      PublicInputSerializer* serializer, const std::map<size_t, size_t>& page_sizes) const;

  /*
    The hash function used to compute the hash of the memory pages in the initial seed.
    Current supported values: keccak, pedersen.
  */
  const std::string page_hash_;
  const std::string layout_name_;
  const uint64_t n_steps_;
  const uint64_t rc_min_;
  const uint64_t rc_max_;
  const MemSegmentAddresses mem_segment_addresses_;
  const std::vector<MemoryAccessUnitData<FieldElementT>> public_memory_;

  std::unique_ptr<Air> air_;
};

}  // namespace cpu
}  // namespace starkware

#endif  //  STARKWARE_STATEMENT_CPU_CPU_AIR_STATEMENT_H_
