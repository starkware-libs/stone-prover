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

#ifndef STARKWARE_AIR_COMPONENTS_BIT_UNPACKING_BIT_UNPACKING_H_
#define STARKWARE_AIR_COMPONENTS_BIT_UNPACKING_BIT_UNPACKING_H_

#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/algebra/big_int.h"

namespace starkware {

template <typename FieldElementT, size_t N>
class BitUnpackingComponent {
 public:
  BitUnpackingComponent(const std::string& name, const TraceGenerationContext& ctx)
      : limit_(ctx.GetObject<BigInt<N>>(name + "/limit")),
        bits_(ctx.GetObject<std::vector<size_t>>(name + "/ones_indices")) {
    // The iteration starts at 1 to skip the MSb which doesn't need its own trace cell.
    for (size_t i = 1; i < bits_.size(); ++i) {
      cumulative_bit_columns_.push_back(
          ctx.GetVirtualColumn(name + "/prod_ones" + std::to_string(bits_[i])));
    }
  }

  /*
    Writes the trace for one instance of the component.
    num is an integer in the range [0, limit).
  */
  void WriteTrace(
      const uint64_t component_index, const gsl::span<const gsl::span<FieldElementT>> trace,
      BigInt<N> num, bool disable_asserts = false) const {
    if (!disable_asserts) {
      ASSERT_RELEASE(num < limit_, "The number must be lower than the limit.");
    }
    std::vector<bool> num_bits = num.ToBoolVector();
    uint64_t cumulative_product = num_bits[bits_[0]] ? 1 : 0;
    // The iteration starts at 1 to skip the MSb which doesn't need its own trace cell.
    for (size_t i = 1; i < bits_.size(); ++i) {
      cumulative_product *= num_bits[bits_[i]] ? 1 : 0;
      cumulative_bit_columns_[i - 1].SetCell(
          trace, component_index, FieldElementT::FromUint(cumulative_product));
    }
  }

 private:
  /*
    The limit under which the number should be.
  */
  BigInt<N> limit_;

  /*
    The indices of the 1s in the limit.
  */
  std::vector<size_t> bits_;

  /*
    The columns representing the cumulative products of bits.
  */
  std::vector<VirtualColumn> cumulative_bit_columns_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_COMPONENTS_BIT_UNPACKING_BIT_UNPACKING_H_
