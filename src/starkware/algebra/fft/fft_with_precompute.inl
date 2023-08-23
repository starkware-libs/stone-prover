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

#include "starkware/algebra/fft/details.h"
#include "starkware/algebra/fft/fft_with_precompute.h"

namespace starkware {

template <typename BasesT>
void FftWithPrecompute<BasesT>::Fft(
    const gsl::span<const FieldElementT> src, const gsl::span<FieldElementT> dst) const {
  if constexpr (BasesT::kOrder == MultiplicativeGroupOrdering::kNaturalOrder) {  // NOLINT
    FftNaturalOrder(src, dst);
  } else {  // NOLINT
    FftReversedOrder(src, dst);
  }
}

template <typename BasesT>
void FftWithPrecompute<BasesT>::FftNaturalOrder(
    const gsl::span<const FieldElementT> src, const gsl::span<FieldElementT> dst) const {
  gsl::span<const FieldElementT> curr_src = src;
  size_t precompute_depth = PrecomputeDepth();
  const size_t last_precomputed_layer_size =  // The succeeding layers use FftNoPrecompute.
      Pow2(precompute_depth);

  if (src.size() == 1) {
    dst[0] = src[0];
    return;
  }

  bool full_precompute = src.size() <= last_precomputed_layer_size;
  if (last_precomputed_layer_size > 1) {
    for (size_t i = 0; i < src.size(); i += last_precomputed_layer_size) {
      fft::details::FftUsingPrecomputedTwiddleFactors<FieldElementT>(
          curr_src.subspan(i, last_precomputed_layer_size), twiddle_factors_,
          /*normalize=*/full_precompute, dst.subspan(i, last_precomputed_layer_size));
    }
    curr_src = dst;
  }

  if (!full_precompute) {
    fft::details::FftNoPrecompute<BasesT>(
        curr_src, bases_,
        /*layers_to_skip=*/precompute_depth, dst);
  }
}

template <typename BasesT>
void FftWithPrecompute<BasesT>::FftReversedOrder(
    const gsl::span<const FieldElementT> src, const gsl::span<FieldElementT> dst) const {
  ASSERT_RELEASE(
      twiddle_factors_.size() + 1 == src.size() || src.size() == 1,
      "only full precompute is currently supported");
  fft::details::FftNaturalToReverseWithPrecompute<FieldElementT>(src, twiddle_factors_, dst);
}

template <typename BasesT>
size_t FftWithPrecompute<BasesT>::PrecomputeDepth() const {
  // The number of TwiddleFactors is 1+2+4+...+2^(Precompute_depth -1) = 2^Precompute_depth - 1.
  return SafeLog2(twiddle_factors_.size() + 1);
}

}  // namespace starkware
