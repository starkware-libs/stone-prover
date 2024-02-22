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

#ifndef STARKWARE_AIR_COMPONENTS_RANGE_CHECK_RANGE_CHECK_H_
#define STARKWARE_AIR_COMPONENTS_RANGE_CHECK_RANGE_CHECK_H_

#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"

namespace starkware {

template <typename FieldElementT>
class RangeCheckComponent {
 public:
  RangeCheckComponent(
      const std::string& name, const TraceGenerationContext& ctx, uint64_t component_height)
      : component_height_(component_height), column_(ctx.GetVirtualColumn(name + "/column")) {}

  /*
    Writes the trace for one instance of the component.
    value is an integer in the range [0, 2^n_bits).
    row is the first row of the component.
  */
  void WriteTrace(
      const uint64_t value, const uint64_t component_index,
      const gsl::span<const gsl::span<FieldElementT>> trace) const {
    ASSERT_RELEASE(column_.column < trace.size(), "Invalid column index");
    const uint64_t row_offset = component_index * component_height_;
    for (uint64_t j = 0; j < component_height_; j++) {
      const uint64_t shifted_value = (j < 64) ? (value >> j) : 0;
      column_.SetCell(trace, row_offset + j, FieldElementT::FromUint(shifted_value));
    }
  }

 private:
  /*
    The period of the component (inside the virtual column).
  */
  const uint64_t component_height_;

  /*
    The virtual column in which the component is located.
  */
  const VirtualColumn column_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_COMPONENTS_RANGE_CHECK_RANGE_CHECK_H_
