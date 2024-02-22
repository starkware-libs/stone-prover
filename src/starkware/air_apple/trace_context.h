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

#ifndef STARKWARE_AIR_TRACE_CONTEXT_H_
#define STARKWARE_AIR_TRACE_CONTEXT_H_

#include <memory>
#include <utility>
#include <vector>

#include "starkware/air/trace.h"

namespace starkware {

/*
  Initializes parameters for trace generation. The motivation is initializing the parameters
  relevant to the trace when creating the statement, before generating the trace itself in
  ProveStark method.
*/
class TraceContext {
 public:
  virtual ~TraceContext() = default;

  /*
    Creates a trace and returns a unique pointer to it.
  */
  virtual Trace GetTrace() = 0;

  /*
    Updates AIR with given interaction elements if this AIR has an interaction. If not - throws an
    error.
  */
  virtual void SetInteractionElements(const FieldElementVector& /*interaction_elms*/) {
    ASSERT_RELEASE(false, "Calling SetInteractionElements in an air with no interaction.");
  }

  /*
    Creates an interaction trace. In case the AIR doesn't have interaction, throws an error.
  */
  virtual Trace GetInteractionTrace() = 0;

  virtual const Air& GetAir() { ASSERT_RELEASE(false, "GetAir in not implemented."); }
};

}  // namespace starkware

#endif  // STARKWARE_AIR_TRACE_CONTEXT_H_
