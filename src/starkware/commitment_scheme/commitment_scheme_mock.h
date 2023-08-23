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

#ifndef STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_MOCK_H_
#define STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_MOCK_H_

#include <cstddef>
#include <map>
#include <set>
#include <vector>

#include "gmock/gmock.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/commitment_scheme/commitment_scheme.h"

namespace starkware {

class CommitmentSchemeProverMock : public CommitmentSchemeProver {
 public:
  MOCK_CONST_METHOD0(NumSegments, size_t());
  MOCK_CONST_METHOD0(SegmentLengthInElements, uint64_t());
  MOCK_CONST_METHOD0(ElementLengthInBytes, size_t());
  MOCK_METHOD2(
      AddSegmentForCommitment, void(gsl::span<const std::byte> segment_data, size_t segment_index));
  MOCK_METHOD0(Commit, void());
  MOCK_METHOD1(StartDecommitmentPhase, std::vector<uint64_t>(const std::set<uint64_t>& queries));
  MOCK_METHOD1(Decommit, void(gsl::span<const std::byte> elements_data));
};

class CommitmentSchemeVerifierMock : public CommitmentSchemeVerifier {
 public:
  MOCK_METHOD0(ReadCommitment, void());
  MOCK_METHOD1(
      VerifyIntegrity, bool(const std::map<uint64_t, std::vector<std::byte>>& elements_to_verify));
  MOCK_CONST_METHOD0(NumOfElements, uint64_t());
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_MOCK_H_
