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

#ifndef STARKWARE_AIR_CPU_BOARD_MEMORY_SEGMENT_H_
#define STARKWARE_AIR_CPU_BOARD_MEMORY_SEGMENT_H_

#include <map>
#include <string>

#include "starkware/error_handling/error_handling.h"

namespace starkware {
namespace cpu {

/*
  See documentation for MemorySegment in src/starkware/cairo/lang/vm/cairo_run.py.
*/
struct MemorySegment {
  uint64_t begin_addr;
  uint64_t stop_ptr;

  bool operator==(const MemorySegment& other) const {
    return begin_addr == other.begin_addr && stop_ptr == other.stop_ptr;
  }
};

inline std::ostream& operator<<(std::ostream& out, const MemorySegment& memory_segment) {
  return out << "MemorySegment(" << memory_segment.begin_addr << ", " << memory_segment.stop_ptr
             << ")";
}

using MemSegmentAddresses = std::map<std::string, MemorySegment>;

inline const MemorySegment& GetSegment(
    const MemSegmentAddresses& mem_segment_addresses, const std::string& segment_name) {
  auto it = mem_segment_addresses.find(segment_name);
  ASSERT_RELEASE(
      it != mem_segment_addresses.end(),
      segment_name + " segment is missing from mem_segment_addresses.");
  return it->second;
}

}  // namespace cpu
}  // namespace starkware

#endif  // STARKWARE_AIR_CPU_BOARD_MEMORY_SEGMENT_H_
