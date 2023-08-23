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

#ifndef STARKWARE_AIR_DEGREE_THREE_EXAMPLE_DEGREE_THREE_EXAMPLE_TRACE_CONTEXT_H_
#define STARKWARE_AIR_DEGREE_THREE_EXAMPLE_DEGREE_THREE_EXAMPLE_TRACE_CONTEXT_H_

#include <memory>
#include <utility>

#include "starkware/air/air.h"
#include "starkware/air/degree_three_example/degree_three_example_air.h"
#include "starkware/air/trace.h"
#include "starkware/air/trace_context.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

template <typename FieldElementT>
class DegreeThreeExampleTraceContext : public TraceContext {
 public:
  using AirT = DegreeThreeExampleAir<FieldElementT>;

  DegreeThreeExampleTraceContext(
      MaybeOwnedPtr<AirT> air, const FieldElementT witness, const size_t res_claim_index)
      : air_(std::move(air)), witness_(witness), res_claim_index_(res_claim_index) {}

  Trace GetTrace() override {
    return DegreeThreeExampleAir<FieldElementT>::GetTrace(
        witness_, air_->TraceLength(), res_claim_index_);
  }

  Trace GetInteractionTrace() override {
    ASSERT_RELEASE(
        false,
        "Calling GetInteractionTrace from degree three example air that doesn't have interaction.");
  }

 private:
  MaybeOwnedPtr<AirT> air_;
  const FieldElementT witness_;
  const size_t res_claim_index_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_DEGREE_THREE_EXAMPLE_DEGREE_THREE_EXAMPLE_TRACE_CONTEXT_H_
