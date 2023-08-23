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

#ifndef STARKWARE_ALGEBRA_FFT_DETAILS_H_
#define STARKWARE_ALGEBRA_FFT_DETAILS_H_

#include <vector>

#include "gflags/gflags.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fft/multiplicative_group_ordering.h"
#include "starkware/algebra/fft/transpose.h"

DECLARE_uint64(four_step_fft_threshold);
DECLARE_int64(log_min_twiddle_shift_task_size);

namespace starkware {

namespace fft {
namespace details {

template <typename FieldElementT>
static void ValidateFFTSizes(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst, size_t log_n);

template <typename BasesT>
std::vector<typename BasesT::FieldElementT> FftPrecomputeTwiddleFactors(
    const BasesT& bases, size_t precompute_depth);

template <typename FieldElementT>
void FftUsingPrecomputedTwiddleFactors(
    gsl::span<const FieldElementT> src, gsl::span<const FieldElementT> twiddle_factors,
    bool normalize, gsl::span<FieldElementT> dst);

/*
  Computes FFT NaturalToReverse
  Input is in natural order (N), output is in bit reversal order (R).
  If normalize is true, the function normalizes the output FieldElements.
*/
template <typename FieldElementT>
void FftNaturalToReverseWithPrecompute(
    gsl::span<const FieldElementT> src, gsl::span<const FieldElementT> twiddle_factors,
    gsl::span<FieldElementT> dst, bool normalize = true);
template <typename FieldElementT>
void FftNaturalToReverseWithPrecomputeInner(
    gsl::span<const FieldElementT> src, gsl::span<const FieldElementT> twiddle_factors,
    gsl::span<FieldElementT> dst, size_t twiddle_tree_root_index, size_t stop_layer,
    bool normalize = true);

template <typename FieldElementT, typename BasesT>
void ParallelFromOtherTwiddle(
    FieldElementT new_coset_offset, const BasesT& bases, gsl::span<FieldElementT> factors_out);

template <typename BasesT>
void FftNoPrecompute(
    gsl::span<const typename BasesT::FieldElementT> src, const BasesT& bases, size_t layers_to_skip,
    gsl::span<typename BasesT::FieldElementT> dst);

}  // namespace details
}  // namespace fft

}  // namespace starkware

#include "starkware/algebra/fft/details.inl"

#endif  // STARKWARE_ALGEBRA_FFT_DETAILS_H_
