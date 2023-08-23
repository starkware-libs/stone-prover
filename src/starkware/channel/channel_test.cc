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

#include "starkware/channel/channel.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/channel/annotation_scope.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/error_handling/test_utils.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(Channel, NoExpectedAnnotations) {
  NoninteractiveVerifierChannel channel(Prng::New(), {});

  AnnotationScope scope(&channel, "scope");
  channel.GetAndSendRandomNumber(1, "first");
  channel.GetAndSendRandomNumber(2, "second");
}

TEST(Channel, ExpectedAnnotations) {
  NoninteractiveVerifierChannel channel(Prng::New(), {});

  channel.SetExpectedAnnotations({"V->P: /scope: first: Number(0)\n", "WRONG"});

  AnnotationScope scope(&channel, "scope");
  channel.GetAndSendRandomNumber(1, "first");
  EXPECT_ASSERT(channel.GetAndSendRandomNumber(1, "second"), HasSubstr("Annotation mismatch"));
}

TEST(Channel, ExpectedAnnotationsTooShort) {
  NoninteractiveVerifierChannel channel(Prng::New(), {});

  channel.SetExpectedAnnotations({"V->P: /scope: first: Number(0)\n"});

  AnnotationScope scope(&channel, "scope");
  channel.GetAndSendRandomNumber(1, "first");
  EXPECT_ASSERT(channel.GetAndSendRandomNumber(1, "second"), HasSubstr("too short"));
}

TEST(Channel, IgnoreAnnotations) {
  NoninteractiveVerifierChannel channel(Prng::New(), {});

  AnnotationScope scope(&channel, "scope");
  channel.GetAndSendRandomNumber(1, "first");
  ASSERT_EQ(channel.GetAnnotations().size(), 1U);
  channel.DisableAnnotations();
  channel.GetAndSendRandomNumber(2, "second");
  ASSERT_EQ(channel.GetAnnotations().size(), 1U);
}

}  // namespace
}  // namespace starkware
