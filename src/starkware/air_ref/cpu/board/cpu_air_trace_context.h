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

#ifndef STARKWARE_AIR_CPU_BOARD_CPU_AIR_TRACE_CONTEXT_H_
#define STARKWARE_AIR_CPU_BOARD_CPU_AIR_TRACE_CONTEXT_H_

#include <memory>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/cpu/board/cpu_air.h"
#include "starkware/air/trace.h"
#include "starkware/air/trace_context.h"
#include "starkware/algebra/polymorphic/field_element_vector.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT, int LayoutId>
class CpuAirTraceContext : public TraceContext {
 public:
  using AirT = CpuAir<FieldElementT, LayoutId>;

  explicit CpuAirTraceContext(
      MaybeOwnedPtr<AirT> air, std::vector<TraceEntry<FieldElementT>> cpu_trace,
      MaybeOwnedPtr<CpuMemory<FieldElementT>> cpu_memory, JsonValue private_input)
      : air_(std::move(air)),
        cpu_trace_(std::move(cpu_trace)),
        cpu_memory_(std::move(cpu_memory)),
        private_input_(std::move(private_input)) {}

  Trace GetTrace() override {
    ASSERT_RELEASE(!cpu_trace_.empty(), "cpu_trace_ is empty. Did you call GetTrace() twice?");
    // NOLINTNEXTLINE (auto [...]).
    auto [cpu_air_prover_ctx1, first_trace] =
        air_->GetTrace(cpu_trace_, std::move(cpu_memory_), private_input_);
    cpu_air_prover_context1_.emplace(std::move(cpu_air_prover_ctx1));
    // cpu_trace_ is not required anymore.
    cpu_trace_ = {};

    return std::move(first_trace);
  }

  void SetInteractionElements(const FieldElementVector& interaction_elms) override {
    ASSERT_RELEASE(function_call_indicator_ == 0, "Interaction air was already set.");
    function_call_indicator_ += 1;
    const auto& interaction_elms_vec = interaction_elms.As<FieldElementT>();
    air_ = UseMovedValue(air_->WithInteractionElementsImpl(interaction_elms_vec));
  }

  Trace GetInteractionTrace() override {
    // Make sure this function is called only once.
    ASSERT_RELEASE(
        function_call_indicator_ == 1,
        "Invalid call to GetInteractionTrace. function_call_indicator_ = " +
            std::to_string(function_call_indicator_) + "!= 1");

    function_call_indicator_ += 1;
    return air_->GetInteractionTrace(ConsumeOptional(std::move(cpu_air_prover_context1_)));
  }

  const Air& GetAir() override { return *air_; }

 private:
  MaybeOwnedPtr<const AirT> air_;
  std::vector<TraceEntry<FieldElementT>> cpu_trace_;
  MaybeOwnedPtr<CpuMemory<FieldElementT>> cpu_memory_;
  JsonValue private_input_;
  std::optional<CpuAirProverContext1<FieldElementT>> cpu_air_prover_context1_;
  // Indicator that makes sure no function was called twice or not in order.
  // SetInteractionElements should be called once (and only once), and only after that
  // GetInteractionTrace should be called once (and only once).
  size_t function_call_indicator_ = 0;
};

}  // namespace cpu
}  // namespace starkware

#endif  // STARKWARE_AIR_CPU_BOARD_CPU_AIR_TRACE_CONTEXT_H_
