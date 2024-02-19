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

#ifndef STARKWARE_AIR_CPU_BOARD_CPU_AIR_H_
#define STARKWARE_AIR_CPU_BOARD_CPU_AIR_H_

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/diluted_check/diluted_check.h"
#include "starkware/air/components/ecdsa/ecdsa.h"
#include "starkware/air/components/memory/memory.h"
#include "starkware/air/components/pedersen_hash/pedersen_hash.h"
#include "starkware/air/components/perm_range_check/perm_range_check.h"
#include "starkware/air/cpu/board/cpu_air_definition.h"
#include "starkware/air/cpu/component/cpu_component.h"
#include "starkware/air/trace.h"
#include "starkware/utils/json.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {
namespace cpu {

/*
  Represents the values of a specific memory access unit inside the AIR.
*/
template <typename FieldElementT>
struct MemoryAccessUnitData {
  uint64_t address;
  FieldElementT value;
  size_t page;

  bool operator==(const MemoryAccessUnitData& other) const {
    return address == other.address && value == other.value && page == other.page;
  }

  static MemoryAccessUnitData<FieldElementT> FromJson(const JsonValue& json) {
    return {
        json["address"].AsUint64(),
        json["value"].AsFieldElement<FieldElementT>(),
        json["page"].AsSizeT(),
    };
  }
};

template <typename FieldElementT>
std::ostream& operator<<(
    std::ostream& out, const MemoryAccessUnitData<FieldElementT>& memory_unit_data) {
  out << "{ address: " << memory_unit_data.address << ", value: " << memory_unit_data.value << " }";
  return out;
}

template <typename FieldElementT>
struct CpuAirProverContext1 {
  MemoryComponentProverContext1<FieldElementT> memory_prover_context1;
  PermRangeCheckComponentProverContext1<FieldElementT> perm_range_check_prover_context1;
  std::optional<DilutedCheckComponentProverContext1<FieldElementT>> diluted_check_prover_context1;
};

template <typename FieldElementT, int LayoutId>
class CpuAir : public CpuAirDefinition<FieldElementT, LayoutId> {
 public:
  using Builder = typename CpuAirDefinition<FieldElementT, LayoutId>::Builder;
  using HashFactoryT = PedersenHashFactory<FieldElementT>;
  using SigConfigT = typename EcdsaComponent<FieldElementT>::Config;

  /*
    Constructs a CpuAir object.

    mem_segment_addresses contains the initial addresses of the memory segments. For example,
    mem_segment_addresses["program"] is the address the program is loaded to (and the initial
    value of pc).
  */
  explicit CpuAir(
      const uint64_t n_steps, std::vector<MemoryAccessUnitData<FieldElementT>> public_memory,
      const uint64_t rc_min, const uint64_t rc_max,
      const MemSegmentAddresses& mem_segment_addresses)
      : CpuAirDefinition<FieldElementT, LayoutId>(
            n_steps * this->kCpuComponentHeight, FieldElementT::FromUint(rc_min),
            FieldElementT::FromUint(rc_max), mem_segment_addresses,
            GetStandardPedersenHashContext()),
        n_steps_(n_steps),
        public_memory_(std::move(public_memory)),
        rc_min_(rc_min),
        rc_max_(rc_max),
        ctx_(this->GetTraceGenerationContext()),
        cpu_component_("cpu", ctx_) {
    ASSERT_RELEASE(
        rc_max_ < Pow2(CpuAir::kOffsetBits), "Invalid value for rc_max: Must be less than " +
                                                 std::to_string(Pow2(CpuAir::kOffsetBits)) + ".");
    ASSERT_RELEASE(rc_min_ <= rc_max_, "Invalid value for rc_max: Must be >= rc_min.");
  }

  void BuildPeriodicColumns(const FieldElementT& gen, Builder* builder) const override;

  std::unique_ptr<const Air> WithInteractionElements(
      const FieldElementVector& interaction_elms) const override {
    const auto& interaction_elms_vec = interaction_elms.As<FieldElementT>();
    return std::make_unique<CpuAir>(WithInteractionElementsImpl(interaction_elms_vec));
  }

  CpuAir WithInteractionElementsImpl(gsl::span<const FieldElementT> interaction_elms) const;

  /*
    Generates the AIR trace for the given Cairo trace entries.
  */
  std::pair<CpuAirProverContext1<FieldElementT>, Trace> GetTrace(
      gsl::span<const TraceEntry<FieldElementT>> cpu_trace,
      MaybeOwnedPtr<CpuMemory<FieldElementT>> memory, const JsonValue& private_input) const;

  /*
    Generates the interaction trace (second trace).
  */
  Trace GetInteractionTrace(CpuAirProverContext1<FieldElementT>&& cpu_air_prover_context1) const;

  uint64_t GetRcMin() const { return rc_min_; }
  uint64_t GetRcMax() const { return rc_max_; }

  /*
    Disables some asserts in the memory component. Should only be used in tests.
  */
  void DisableAssertsForTest() { this->disable_asserts_in_memory_ = true; }

 private:
  constexpr uint64_t PedersenRatio() const;
  constexpr uint64_t RangeCheckRatio() const;
  constexpr uint64_t EcdsaRatio() const;
  constexpr uint64_t BitwiseRatio() const;
  constexpr uint64_t EcOpRatio() const;

  /*
    Writes public_memory virtual column in the trace.
  */
  void WritePublicMemory(
      MemoryCell<FieldElementT>* memory_pool,
      gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    Computes and returns the product representing the value of the public memory.
  */
  FieldElementT GetPublicMemoryProd() const;

  const size_t n_steps_;
  std::vector<MemoryAccessUnitData<FieldElementT>> public_memory_;
  const uint64_t rc_min_;
  const uint64_t rc_max_;

  const TraceGenerationContext ctx_;

  CpuComponent<FieldElementT> cpu_component_;

  // Builtins.
  const HashFactoryT hash_factory_{"pedersen/points", false, GetStandardPedersenHashContext()};

  /*
    Indicator for tests only, disables asserts in WriteTrace() of memory component.
  */
  bool disable_asserts_in_memory_ = false;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/board/cpu_air.inl"

#endif  // STARKWARE_AIR_CPU_BOARD_CPU_AIR_H_
