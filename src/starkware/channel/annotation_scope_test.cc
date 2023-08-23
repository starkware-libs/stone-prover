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

#include "starkware/channel/annotation_scope.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polymorphic/field.h"
#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/crypt_tools/blake2s.h"

namespace starkware {
namespace {

using testing::HasSubstr;

TEST(AnnotationScope, BasicTest) {
  Field test_field = Field::Create<TestFieldElement>();
  Prng prng;
  NoninteractiveProverChannel prover_channel(Prng::New());

  {
    AnnotationScope scope(&prover_channel, "STARK");
    {
      AnnotationScope scope(&prover_channel, "FRI");
      {
        AnnotationScope scope(&prover_channel, "Layer 1");
        {
          AnnotationScope scope(&prover_channel, "Commitment");
          Blake2s256 pcommitment1 = prng.RandomHash<Blake2s256>();
          prover_channel.template SendCommitmentHash<Blake2s256>(pcommitment1, "First FRI layer");
        }
        {
          AnnotationScope scope(&prover_channel, "Eval point");
          FieldElement ptest_field_element1 =
              prover_channel.ReceiveFieldElement(test_field, "evaluation point");
          FieldElement ptest_field_element2 =
              prover_channel.ReceiveFieldElement(test_field, "2nd evaluation point");
        }
      }
      {
        AnnotationScope scope(&prover_channel, "Last Layer");
        {
          AnnotationScope scope(&prover_channel, "Commitment");
          FieldElement pexpected_last_layer_const = test_field.RandomElement(&prng);

          prover_channel.SendFieldElement(pexpected_last_layer_const, "expected last layer const");
        }
        {
          AnnotationScope scope(&prover_channel, "Query");
          prover_channel.ReceiveNumber(8, "index #1");
          prover_channel.ReceiveNumber(8, "index #2");
        }
      }
      {
        AnnotationScope scope(&prover_channel, "Decommitment");
        for (size_t i = 0; i < 15; i++) {
          prover_channel.SendDecommitmentNode<Blake2s256>(
              prng.RandomHash<Blake2s256>(), "FRI layer");
        }
      }
    }
    DLOG(INFO) << prover_channel;
    std::ostringstream output;
    output << prover_channel;
    EXPECT_THAT(output.str(), HasSubstr("/STARK/FRI/Layer 1/Commitment"));
    EXPECT_THAT(output.str(), HasSubstr("/STARK/FRI/Layer 1/Eval point"));
    EXPECT_THAT(output.str(), HasSubstr("/STARK/FRI/Decommitment"));
  }
}

}  // namespace
}  // namespace starkware
