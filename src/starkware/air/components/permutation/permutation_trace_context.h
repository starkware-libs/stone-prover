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

#ifndef STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_TRACE_CONTEXT_H_
#define STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_TRACE_CONTEXT_H_

#include <memory>
#include <utility>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/components/permutation/permutation_dummy_air.h"
#include "starkware/air/trace.h"
#include "starkware/air/trace_context.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

template <typename FieldElementT>
class PermutationTraceContext : public TraceContext {
 public:
  using AirT = PermutationDummyAir<FieldElementT, 0>;

  explicit PermutationTraceContext(MaybeOwnedPtr<AirT> air) : air_(std::move(air)) {}

  Trace GetTrace() override {
    Trace trace = air_->GetTrace();
    // Initialize interaction_data_ with relevant data to interaction trace.
    for (size_t i = 0; i < AirT::kNOriginalCols; i++) {
      originals_.push_back(trace.GetColumn(i).template As<FieldElementT>());
      perms_.push_back(trace.GetColumn(i + AirT::kNOriginalCols).template As<FieldElementT>());
    }
    return trace;
  }

  void SetInteractionElements(const FieldElementVector& interaction_elms) override {
    ASSERT_RELEASE(function_call_indicator_ == 0, "Interaction air was already set.");
    function_call_indicator_ += 1;
    const auto& interaction_elms_vec = interaction_elms.As<FieldElementT>();
    air_ = TakeOwnershipFrom(air_->WithInteractionElementsImpl(interaction_elms_vec));
  }

  Trace GetInteractionTrace() override {
    // Make sure this function is called only once.
    ASSERT_RELEASE(
        function_call_indicator_ == 1,
        "Invalid call to GetInteractionTrace. function_call_indicator_ = " +
            std::to_string(function_call_indicator_) + "!= 1");
    // Validate size of interaction data.
    ASSERT_RELEASE(
        originals_.size() == AirT::kNOriginalCols && perms_.size() == AirT::kNOriginalCols,
        "Interaction data is of wrong size.");

    auto trace = air_->GetInteractionTrace(
        ConstSpanAdapter<FieldElementT>(originals_), ConstSpanAdapter<FieldElementT>(perms_));
    function_call_indicator_ += 1;
    return trace;
  }

  const Air& GetAir() override { return *air_; }

  void SetInteractionElementsForTest(const FieldElementVector& interaction_elms) {
    air_ =
        TakeOwnershipFrom(air_->WithInteractionElementsImpl(interaction_elms.As<FieldElementT>()));
  }

 private:
  MaybeOwnedPtr<const AirT> air_;
  std::vector<std::vector<FieldElementT>> originals_;
  std::vector<std::vector<FieldElementT>> perms_;
  // Indicator that makes sure no function was called twice or not in order.
  // SetInteractionElements should be called once (and only once), and only after that
  // GetInteractionTrace should be called once (and only once).
  size_t function_call_indicator_ = 0;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_TRACE_CONTEXT_H_
