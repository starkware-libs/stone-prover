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

#ifndef STARKWARE_CHANNEL_VERIFIER_CHANNEL_MOCK_H_
#define STARKWARE_CHANNEL_VERIFIER_CHANNEL_MOCK_H_

#include <string>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/channel/verifier_channel.h"

namespace starkware {

class VerifierChannelMock : public VerifierChannel {
 public:
  MOCK_METHOD1(SendNumberImpl, void(uint64_t number));
  MOCK_METHOD1(SendFieldElementImpl, void(const FieldElement& value));
  MOCK_METHOD1(GetAndSendRandomNumberImpl, uint64_t(uint64_t upper_bound));
  MOCK_METHOD1(GetAndSendRandomFieldElementImpl, FieldElement(const Field& field));
  MOCK_METHOD1(ReceiveFieldElementImpl, FieldElement(const Field& field));
  MOCK_METHOD2(ReceiveFieldElementSpanImpl, void(const Field& field, const FieldElementSpan& span));
  // ReceiveCommitmentHash() cannot be mocked since it is a template function and therefore cannot
  // be virtual. Use EXPECT_CALL(..., ReceiveBytes(...)) instead.

  MOCK_METHOD1(SendBytes, void(const gsl::span<const std::byte> raw_bytes));
  MOCK_METHOD1(ReceiveBytes, std::vector<std::byte>(const size_t num_bytes));
  MOCK_METHOD1(GetRandomNumber, uint64_t(uint64_t upper_bound));
  MOCK_METHOD1(GetRandomFieldElement, FieldElement(const Field& field));
  MOCK_METHOD1(ApplyProofOfWork, void(size_t));
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_VERIFIER_CHANNEL_MOCK_H_
