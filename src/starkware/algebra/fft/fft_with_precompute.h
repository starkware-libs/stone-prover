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

#ifndef STARKWARE_ALGEBRA_FFT_FFT_WITH_PRECOMPUTE_H_
#define STARKWARE_ALGEBRA_FFT_FFT_WITH_PRECOMPUTE_H_

#include <algorithm>
#include <utility>
#include <vector>

#include "starkware/algebra/fft/details.h"

namespace starkware {

namespace fft_tuning_params {
const size_t kPrecomputeDepth = 22;  // Found empirically though benchmarking.
}  // namespace fft_tuning_params

class FftWithPrecomputeBase {
 public:
  virtual ~FftWithPrecomputeBase() = default;
  virtual void ShiftTwiddleFactors(const FieldElement& offset, const FieldElement& prev_offset) = 0;
};

/*
  Make this a mock if FftWithPrecomputeBase becomes more complex.
*/
class DummyFftWithPrecompute : public FftWithPrecomputeBase {
  void ShiftTwiddleFactors(
      const FieldElement& /*offset*/, const FieldElement& /*prev_offset*/) override {}
};

template <typename BasesT>
class FftWithPrecompute : public FftWithPrecomputeBase {
  using FieldElementT = typename BasesT::FieldElementT;

 public:
  explicit FftWithPrecompute(BasesT bases, size_t precompute_depth)
      : bases_(std::move(bases)),
        twiddle_factors_(fft::details::FftPrecomputeTwiddleFactors<BasesT>(
            bases_, std::min(precompute_depth, bases_.NumLayers()))) {}

  explicit FftWithPrecompute(BasesT bases) : FftWithPrecompute(bases, bases.NumLayers()) {}

  void Fft(gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst) const;

  // Returns the number of FFT layers whose TwiddleFactors were precomputed.
  size_t PrecomputeDepth() const;

  const std::vector<FieldElementT>& GetTwiddleFactors() const { return twiddle_factors_; }

  // Shifts the twiddle factors by c, to accommodate for evaluation.
  void ShiftTwiddleFactors(const FieldElement& offset, const FieldElement& prev_offset) override {
    if (twiddle_factors_.empty()) {
      return;
    }
    fft::details::ParallelFromOtherTwiddle<FieldElementT, BasesT>(
        BasesT::GroupT::GroupOperation(
            offset.As<FieldElementT>(),
            BasesT::GroupT::GroupOperationInverse(prev_offset.As<FieldElementT>())),
        bases_, twiddle_factors_);
  }

 private:
  void FftNaturalOrder(gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst) const;
  void FftReversedOrder(gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst) const;

  const BasesT bases_;
  std::vector<FieldElementT> twiddle_factors_;
};

}  // namespace starkware

#include "starkware/algebra/fft/fft_with_precompute.inl"

#endif  // STARKWARE_ALGEBRA_FFT_FFT_WITH_PRECOMPUTE_H_
