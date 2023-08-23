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

#ifndef STARKWARE_CHANNEL_ANNOTATION_SCOPE_H_
#define STARKWARE_CHANNEL_ANNOTATION_SCOPE_H_

#include <string>

#include "starkware/channel/channel.h"

namespace starkware {

/*
  This class is used in order to generate scoped annotations in the prover and verifier channels.
  In order to use it, in a given scope, call it with the desired string which is then concatenated
  to every annotation added to the channel as a "/" delimited string (i.e. like a linux path).
  A scope is left by the side effect of the destruction of the annotation object which implies that
  no code needs to be written in order to leave a scope.

  For a usage example, please see annotation_scope_test.cc which results in the following output:

  P->V: /STARK/FRI/Layer 1/Commitment: First FRI layer: Commitment...
  V->P: /STARK/FRI/Layer 1/Eval point: evaluation point: Field Element...
  V->P: /STARK/FRI/Layer 1/Eval point: 2nd evaluation point: Field Element...
  P->V: /STARK/FRI/Last Layer/Commitment: expected last layer const: Field Element...
  V->P: /STARK/FRI/Last Layer/Query: index #1: Number...
  V->P: /STARK/FRI/Last Layer/Query: index #2: Number...
  P->V: /STARK/FRI/Decommitment: FRI layer: Decommitment: ...
*/
class AnnotationScope {
 public:
  /*
    Enters an annotation scope (new prefix added to annotation printouts).
  */
  explicit AnnotationScope(Channel* channel, const std::string& scope) : channel_(channel) {
    channel->EnterAnnotationScope(scope);
  }

  /*
    Exits the annotation scope by sideeffect of destruction.
  */
  ~AnnotationScope() { channel_->ExitAnnotationScope(); }

  AnnotationScope(const AnnotationScope&) = delete;
  AnnotationScope& operator=(const AnnotationScope&) = delete;
  AnnotationScope(AnnotationScope&& other) = delete;
  AnnotationScope& operator=(AnnotationScope&& other) = delete;

 private:
  Channel* channel_;
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_ANNOTATION_SCOPE_H_
