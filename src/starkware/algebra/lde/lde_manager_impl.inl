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

#include "starkware/algebra/lde/lde_manager_impl.h"
#include "starkware/utils/maybe_owned_ptr.h"

#include "third_party/cppitertools/zip.hpp"

#include "starkware/algebra/lde/multiplicative_lde.h"
#include "starkware/utils/profiling.h"

namespace starkware {

template <typename LdeT>
LdeManagerTmpl<LdeT>::LdeManagerTmpl(BasesT bases)
    : bases_(std::move(bases)),
      lde_size_(Pow2(bases_.NumLayers())),
      offset_compensation_(
          bases_.NumLayers() > 0 ? bases_[0].StartOffset().Inverse() : FieldElementT::One()) {}

template <typename LdeT>
void LdeManagerTmpl<LdeT>::AddEvaluation(
    FieldElementVector&& evaluation, FftWithPrecomputeBase* fft_precomputed) {
  AddEvaluation(std::move(evaluation.As<FieldElementT>()), fft_precomputed);
}

template <typename LdeT>
void LdeManagerTmpl<LdeT>::AddEvaluation(
    const ConstFieldElementSpan& evaluation, FftWithPrecomputeBase* fft_precomputed) {
  AddEvaluation(evaluation.As<FieldElementT>(), fft_precomputed);
}

template <typename LdeT>
void LdeManagerTmpl<LdeT>::AddEvaluation(
    gsl::span<const FieldElementT> evaluation, FftWithPrecomputeBase* fft_precomputed) {
  AddEvaluation(std::vector<FieldElementT>(evaluation.begin(), evaluation.end()), fft_precomputed);
}

template <typename LdeT>
void LdeManagerTmpl<LdeT>::AddEvaluation(
    std::vector<FieldElementT> evaluation, FftWithPrecomputeBase* fft_precomputed) {
  ldes_vector_.push_back(LdeT::AddFromEvaluation(bases_, std::move(evaluation), fft_precomputed));
}

template <typename LdeT>
void LdeManagerTmpl<LdeT>::EvalOnCoset(
    const FieldElement& coset_offset, gsl::span<const FieldElementSpan> evaluation_results,
    FftWithPrecomputeBase* fft_precomputed, TaskManager* task_manager) const {
  ASSERT_RELEASE(
      ldes_vector_.size() == evaluation_results.size(),
      "evaluation_results.size() must match number of LDEs.");

  for (const auto& column : evaluation_results) {
    ASSERT_RELEASE(column.Size() == bases_[0].Size(), "Wrong column output size");
  }

  MaybeOwnedPtr<typename LdeT::PrecomputeType> maybe_precomputed =
      UseOwned(dynamic_cast<typename LdeT::PrecomputeType*>(fft_precomputed));

  if (fft_precomputed == nullptr) {
    maybe_precomputed = UseMovedValue(
        LdeT::FftPrecompute(bases_, offset_compensation_, coset_offset.As<FieldElementT>()));
  }

  task_manager->ParallelFor(
      ldes_vector_.size(), [&maybe_precomputed, &ldes = this->ldes_vector_,
                            evaluation_results](const TaskInfo& task_info) {
        size_t idx = task_info.start_idx;
        ldes[idx].EvalAtCoset(
            *maybe_precomputed, evaluation_results[idx].template As<FieldElementT>());
      });
}

template <typename LdeT>
std::unique_ptr<FftWithPrecomputeBase> LdeManagerTmpl<LdeT>::FftPrecompute(
    const FieldElement& coset_offset) const {
  return std::make_unique<typename LdeT::PrecomputeType>(
      LdeT::FftPrecompute(bases_, offset_compensation_, coset_offset.As<FieldElementT>()));
}

template <typename LdeT>
std::unique_ptr<FftWithPrecomputeBase> LdeManagerTmpl<LdeT>::IfftPrecompute() const {
  return LdeT::IfftPrecompute(bases_);
}

template <typename LdeT>
void LdeManagerTmpl<LdeT>::EvalOnCoset(
    const FieldElement& coset_offset, gsl::span<const FieldElementSpan> evaluation_results,
    FftWithPrecomputeBase* fft_precomputed) const {
  EvalOnCoset(coset_offset, evaluation_results, fft_precomputed, &TaskManager::GetInstance());
}

template <typename LdeT>
void LdeManagerTmpl<LdeT>::EvalOnCoset(
    const FieldElement& coset_offset, gsl::span<const FieldElementSpan> evaluation_results) const {
  EvalOnCoset(coset_offset, evaluation_results, nullptr);
}

template <typename LdeT>
void LdeManagerTmpl<LdeT>::AddFromCoefficients(const ConstFieldElementSpan& coefficients) {
  ASSERT_RELEASE(
      coefficients.Size() == lde_size_,
      "Expected number of coefficients to be: " + std::to_string(lde_size_) +
          ". Actual: " + std::to_string(coefficients.Size()));
  const gsl::span<const FieldElementT> coef_span = coefficients.As<FieldElementT>();
  ldes_vector_.push_back(
      LdeT::AddFromCoefficients(std::vector<FieldElementT>(coef_span.begin(), coef_span.end())));
}

template <typename LdeT>
void LdeManagerTmpl<LdeT>::EvalAtPoints(
    size_t evaluation_idx, const ConstFieldElementSpan& points,
    const FieldElementSpan& outputs) const {
  return EvalAtPoints(evaluation_idx, points.As<FieldElementT>(), outputs.As<FieldElementT>());
}
template <typename LdeT>
void LdeManagerTmpl<LdeT>::EvalAtPoints(
    size_t evaluation_idx, gsl::span<const FieldElementT> points,
    gsl::span<FieldElementT> outputs) const {
  ASSERT_RELEASE(evaluation_idx < ldes_vector_.size(), "evaluation_idx out of range.");
  std::vector<FieldElementT> fixed_points;
  fixed_points.reserve(points.size());
  for (const FieldElementT& point : points) {
    fixed_points.push_back(point * offset_compensation_);
  }
  ldes_vector_[evaluation_idx].EvalAtPoints(fixed_points, outputs);
}

template <typename LdeT>
int64_t LdeManagerTmpl<LdeT>::GetEvaluationDegree(size_t evaluation_idx) const {
  ASSERT_RELEASE(evaluation_idx < ldes_vector_.size(), "evaluation_idx out of range.");
  return ldes_vector_[evaluation_idx].GetDegree();
}

template <typename LdeT>
ConstFieldElementSpan LdeManagerTmpl<LdeT>::GetCoefficients(size_t evaluation_idx) const {
  ASSERT_RELEASE(evaluation_idx < ldes_vector_.size(), "evaluation_idx out of range.");
  return ConstFieldElementSpan(
      gsl::span<const FieldElementT>(ldes_vector_[evaluation_idx].GetCoefficients()));
}

template <typename LdeT>
std::unique_ptr<FftDomainBase> LdeManagerTmpl<LdeT>::GetDomain(const FieldElement& offset) const {
  return std::make_unique<typename BasesT::DomainT>(
      bases_[0].GetShiftedDomain(offset.As<FieldElementT>()));
}

template <MultiplicativeGroupOrdering Order>
std::unique_ptr<LdeManager> MakeLdeManagerImpl(
    const OrderedGroup& source_domain_group, const FieldElement& source_eval_coset_offset) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) -> std::unique_ptr<LdeManager> {
        using FieldElementT = typename decltype(field_tag)::type;
        FieldElementT offset = source_eval_coset_offset.As<FieldElementT>();
        ASSERT_RELEASE(offset != FieldElementT::Zero(), "lde coset offset can't be zero.");
        const auto& multiplicative_group =
            dynamic_cast<const MultiplicativeGroup&>(source_domain_group);

        return std::make_unique<LdeManagerTmpl<MultiplicativeLde<Order, FieldElementT>>>(
            MakeFftBases<Order>(
                multiplicative_group.Generator().As<FieldElementT>(),
                SafeLog2(multiplicative_group.Size()), offset));
      },
      source_domain_group.GetField());
}

}  // namespace starkware
