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

#ifndef STARKWARE_STARK_COMMITTED_TRACE_MOCK_H_
#define STARKWARE_STARK_COMMITTED_TRACE_MOCK_H_

#include <tuple>
#include <utility>

#include "gmock/gmock.h"

#include "starkware/stark/committed_trace.h"

namespace starkware {

class CommittedTraceProverMock : public CommittedTraceProverBase {
 public:
  MOCK_CONST_METHOD0(NumColumns, size_t());
  MOCK_METHOD0(GetLde, CachedLdeManager*());

  /*
    Workaround GMock has a problem with rvalue reference parameter (&&). So, we implement it and
    pass execution to Commit_rvr (rvr stands for rvalue reference).
  */
  void Commit(Trace&& trace, const FftBases& trace_domain, bool bit_reverse) override {
    Commit_rvr(trace, trace_domain, bit_reverse);
  }
  MOCK_METHOD3(Commit_rvr, void(const Trace&, const FftBases&, bool));
  MOCK_CONST_METHOD1(
      DecommitQueries, void(gsl::span<const std::tuple<uint64_t, uint64_t, size_t>>));
  MOCK_CONST_METHOD3(
      EvalMaskAtPoint, void(
                           gsl::span<const std::pair<int64_t, uint64_t>>, const FieldElement&,
                           const FieldElementSpan&));
  MOCK_METHOD0(FinalizeEval, void());
};

class CommittedTraceVerifierMock : public CommittedTraceVerifierBase {
 public:
  MOCK_CONST_METHOD0(NumColumns, size_t());
  MOCK_METHOD0(ReadCommitment, void());
  MOCK_CONST_METHOD1(
      VerifyDecommitment,
      FieldElementVector(gsl::span<const std::tuple<uint64_t, uint64_t, size_t>>));
};

}  // namespace starkware

#endif  // STARKWARE_STARK_COMMITTED_TRACE_MOCK_H_
