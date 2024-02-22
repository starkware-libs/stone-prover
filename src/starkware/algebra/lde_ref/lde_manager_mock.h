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

#ifndef STARKWARE_ALGEBRA_LDE_LDE_MANAGER_MOCK_H_
#define STARKWARE_ALGEBRA_LDE_LDE_MANAGER_MOCK_H_

#include <memory>
#include <utility>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/algebra/lde/lde.h"

namespace starkware {

class LdeManagerMock : public LdeManager {
 public:
  using DomainT = FftDomain<FftMultiplicativeGroup<TestFieldElement>>;
  explicit LdeManagerMock(DomainT domain) : domain_(std::move(domain)) {}

  /*
    Workaround. GMock has a problem with rvalue reference parameter (&&). So, we implement it and
    pass execution to AddEvaluation_rvr (rvr stands for rvalue reference).
  */
  void AddEvaluation(
      FieldElementVector&& evaluation, FftWithPrecomputeBase* fft_precomputed) override {
    AddEvaluation_rvr(evaluation, fft_precomputed);
  }
  MOCK_METHOD2(
      AddEvaluation_rvr,
      void(const FieldElementVector& evaluation, FftWithPrecomputeBase* fft_precomputed));
  MOCK_METHOD2(
      AddEvaluation,
      void(const ConstFieldElementSpan& evaluation, FftWithPrecomputeBase* fft_precomputed));
  MOCK_CONST_METHOD3(
      EvalOnCoset,
      void(
          const FieldElement& coset_offset, gsl::span<const FieldElementSpan> evaluation_results,
          FftWithPrecomputeBase*));
  MOCK_CONST_METHOD2(
      EvalOnCoset,
      void(const FieldElement& coset_offset, gsl::span<const FieldElementSpan> evaluation_results));
  MOCK_CONST_METHOD3(
      EvalAtPoints, void(
                        size_t evaluation_idx, const ConstFieldElementSpan& points,
                        const FieldElementSpan& outputs));

  MOCK_CONST_METHOD1(
      FftPrecompute, std::unique_ptr<FftWithPrecomputeBase>(const FieldElement& coset_offset));
  MOCK_CONST_METHOD0(IfftPrecompute, std::unique_ptr<FftWithPrecomputeBase>());

  MOCK_METHOD1(AddFromCoefficients, void(const ConstFieldElementSpan& coefficients));
  MOCK_CONST_METHOD1(GetEvaluationDegree, int64_t(size_t evaluation_idx));
  MOCK_CONST_METHOD1(GetCoefficients, ConstFieldElementSpan(size_t evaluation_idx));

  std::unique_ptr<FftDomainBase> GetDomain(const FieldElement& offset) const override {
    return std::make_unique<DomainT>(domain_.GetShiftedDomain(offset.As<TestFieldElement>()));
  }

 private:
  DomainT domain_;
};

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_LDE_LDE_MANAGER_MOCK_H_
