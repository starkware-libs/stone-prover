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

#ifndef STARKWARE_AIR_COMPONENTS_POSEIDON_POSEIDON_H_
#define STARKWARE_AIR_COMPONENTS_POSEIDON_POSEIDON_H_

#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/math/math.h"

namespace starkware {

template <typename FieldElementT>
class PoseidonComponent {
 public:
  /*
    A component for computing the Poseidon hash function.
    See src/starkware/air/components/poseidon/poseidon.py for documentation.
  */
  PoseidonComponent(
      const std::string& name, const TraceGenerationContext& ctx, uint64_t m, uint64_t rounds_full,
      uint64_t rounds_partial, gsl::span<const uint64_t> partial_rounds_partition,
      const ConstSpanAdapter<FieldElementT>& mds, const ConstSpanAdapter<FieldElementT>& ark)
      : m_(m),
        rounds_full_(rounds_full),
        rounds_full_capacity_(Pow2(Log2Ceil(rounds_full))),
        rounds_full_half_capacity_(Pow2(Log2Ceil(rounds_full) - 1)),
        partial_rounds_partition_(partial_rounds_partition.begin(), partial_rounds_partition.end()),
        r_p_capacities_(InitRPCapacities(partial_rounds_partition)),
        full_rounds_state_(GetStateColumns(name + "/full_rounds_state", ctx, "", m)),
        full_rounds_state_squared_(
            GetStateColumns(name + "/full_rounds_state", ctx, "_squared", m)),
        partial_rounds_state_(GetStateColumns(
            name + "/partial_rounds_state", ctx, "", partial_rounds_partition.size())),
        partial_rounds_state_squared_(GetStateColumns(
            name + "/partial_rounds_state", ctx, "_squared", partial_rounds_partition.size())),
        mds_(mds),
        ark_(ark) {
    ASSERT_RELEASE(
        std::accumulate(
            partial_rounds_partition.begin(), partial_rounds_partition.end(), uint64_t(0)) ==
            rounds_partial + m * (partial_rounds_partition.size() - 1),
        "Incompatible partial rounds partition.");
    ASSERT_RELEASE(mds.Size() == m, "Incompatible MDS dimensions.");
    for (uint64_t i = 0; i < m; ++i) {
      ASSERT_RELEASE(mds[i].size() == m, "Incompatible MDS dimensions.");
    }
    ASSERT_RELEASE(ark.Size() == rounds_full + rounds_partial, "Incompatible ARK dimensions.");
    for (uint64_t i = 0; i < rounds_full + rounds_partial; ++i) {
      ASSERT_RELEASE(ark[i].size() == m, "Incompatible ARK dimensions.");
    }
  }

  /*
    Writes the trace for one instance of the component.
    input is input state.
    Returns the outputs as field elements.
  */
  std::vector<FieldElementT> WriteTrace(
      gsl::span<const FieldElementT> input, uint64_t component_index,
      gsl::span<const gsl::span<FieldElementT>> trace) const;

 private:
  static const std::vector<uint64_t> InitRPCapacities(
      gsl::span<const uint64_t> partial_rounds_partition) {
    std::vector<uint64_t> capacities;
    capacities.reserve(partial_rounds_partition.size());
    for (uint64_t size : partial_rounds_partition) {
      capacities.push_back(Pow2(Log2Ceil(size)));
    }
    return capacities;
  }

  static const std::vector<VirtualColumn> GetStateColumns(
      const std::string& name, const TraceGenerationContext& ctx, const std::string& suffix,
      uint64_t size) {
    std::vector<VirtualColumn> state_columns;
    state_columns.reserve(size);
    for (uint64_t i = 0; i < size; ++i) {
      state_columns.push_back(ctx.GetVirtualColumn((name + std::to_string(i)).append(suffix)));
    }
    return state_columns;
  }

  const uint64_t m_;
  const uint64_t rounds_full_;
  const uint64_t rounds_full_capacity_;
  const uint64_t rounds_full_half_capacity_;
  const std::vector<uint64_t> partial_rounds_partition_;
  const std::vector<uint64_t> r_p_capacities_;

  /*
    The virtual columns.
  */
  const std::vector<VirtualColumn> full_rounds_state_;
  const std::vector<VirtualColumn> full_rounds_state_squared_;
  const std::vector<VirtualColumn> partial_rounds_state_;
  const std::vector<VirtualColumn> partial_rounds_state_squared_;

  /*
    The constants.
  */
  const ConstSpanAdapter<FieldElementT> mds_;
  const ConstSpanAdapter<FieldElementT> ark_;
};

}  // namespace starkware

#include "starkware/air/components/poseidon/poseidon.inl"

#endif  // STARKWARE_AIR_COMPONENTS_POSEIDON_POSEIDON_H_
