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

#ifndef STARKWARE_ALGEBRA_LDE_LDE_MANAGER_IMPL_H_
#define STARKWARE_ALGEBRA_LDE_LDE_MANAGER_IMPL_H_

#include <memory>
#include <vector>

#include "starkware/algebra/lde/lde.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

template <typename LdeT>
class LdeManagerTmpl : public LdeManager {
 public:
  using FieldElementT = typename LdeT::T;
  using BasesT = typename LdeT::BasesT;
  explicit LdeManagerTmpl(BasesT bases);

  void AddEvaluation(
      FieldElementVector&& evaluation, FftWithPrecomputeBase* fft_precomputed) override;

  void AddEvaluation(
      const ConstFieldElementSpan& evaluation, FftWithPrecomputeBase* fft_precomputed) override;

  void EvalOnCoset(
      const FieldElement& coset_offset, gsl::span<const FieldElementSpan> evaluation_results,
      FftWithPrecomputeBase* fft_precomputed, TaskManager* task_manager) const;

  void EvalOnCoset(
      const FieldElement& coset_offset, gsl::span<const FieldElementSpan> evaluation_results,
      FftWithPrecomputeBase* fft_precomputed) const override;

  void EvalOnCoset(
      const FieldElement& coset_offset,
      gsl::span<const FieldElementSpan> evaluation_results) const override;

  void AddFromCoefficients(const ConstFieldElementSpan& coefficients) override;

  std::unique_ptr<FftWithPrecomputeBase> FftPrecompute(
      const FieldElement& coset_offset) const override;
  std::unique_ptr<FftWithPrecomputeBase> IfftPrecompute() const override;

  void EvalAtPoints(
      size_t evaluation_idx, const ConstFieldElementSpan& points,
      const FieldElementSpan& outputs) const override;

  int64_t GetEvaluationDegree(size_t evaluation_idx) const override;

  ConstFieldElementSpan GetCoefficients(size_t evaluation_idx) const override;

  void EvalAtPoints(
      size_t evaluation_idx, gsl::span<const FieldElementT> points,
      gsl::span<FieldElementT> outputs) const;

  void AddEvaluation(
      gsl::span<const FieldElementT> evaluation, FftWithPrecomputeBase* fft_precomputed = nullptr);
  void AddEvaluation(
      std::vector<FieldElementT> evaluation, FftWithPrecomputeBase* fft_precomputed = nullptr);

  std::unique_ptr<FftDomainBase> GetDomain(const FieldElement& offset) const override;

 private:
  const BasesT bases_;
  const uint64_t lde_size_;

  // IFFT assumes that its input is given on the unit coset 1 *<g>
  // if the input is given on a different coset c*<g> and we need to compensate
  // for that as follows:
  // The evaluation of p(x) on c*<g> is the same as p(c*x) on <g>
  // Therefore if do an lde for an evaluation on  c*<g>, we will get
  // polynomial p(c*x) rather than p(x) and we will need to use the offset d/c rather
  // just d to get the evaluation on d*<g>.
  const FieldElementT offset_compensation_;

  std::vector<LdeT> ldes_vector_;
};

}  // namespace starkware

#include "starkware/algebra/lde/lde_manager_impl.inl"

#endif  // STARKWARE_ALGEBRA_LDE_LDE_MANAGER_IMPL_H_
