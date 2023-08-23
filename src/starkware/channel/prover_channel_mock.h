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

#ifndef STARKWARE_CHANNEL_PROVER_CHANNEL_MOCK_H_
#define STARKWARE_CHANNEL_PROVER_CHANNEL_MOCK_H_

#include <string>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/channel/prover_channel.h"

namespace starkware {

class ProverChannelMock : public ProverChannel {
 public:
  MOCK_METHOD1(SendFieldElementImpl, void(const FieldElement& value));
  MOCK_METHOD1(SendFieldElementSpanImpl, void(const ConstFieldElementSpan& values));

  // SendCommitmentHash() cannot be mocked since it is a template function and therefore cannot
  // be virtual. Use EXPECT_CALL(..., SendBytes(...)) instead.
  MOCK_METHOD1(ReceiveFieldElementImpl, FieldElement(const Field& field));
  MOCK_METHOD1(ReceiveNumberImpl, uint64_t(uint64_t upper_bound));

  MOCK_METHOD1(SendBytes, void(gsl::span<const std::byte> raw_bytes));
  MOCK_METHOD1(ReceiveBytes, std::vector<std::byte>(const size_t num_bytes));
  MOCK_METHOD1(ApplyProofOfWork, void(size_t));
};

}  // namespace starkware

#endif  // STARKWARE_CHANNEL_PROVER_CHANNEL_MOCK_H_
