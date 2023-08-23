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

#include "starkware/algebra/fft/fft_with_precompute.h"
#include "starkware/algebra/fft/multiplicative_fft.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/fft_utils/fft_bases.h"
#include "starkware/math/math.h"

namespace starkware {

template <typename BasesT>
class FftWithPrecompute;

template <typename BasesT>
void MultiplicativeFft(
    const BasesT& bases, gsl::span<const typename BasesT::FieldElementT> src,
    gsl::span<typename BasesT::FieldElementT> dst) {
  const size_t log_n = SafeLog2(src.size());
  if (log_n == 0) {
    dst[0] = src[0];
    return;
  }

  FftWithPrecompute<BasesT>(bases, fft_tuning_params::kPrecomputeDepth).Fft(src, dst);
}

template <typename FieldElementT>
void FftNaturalToReverse(
    const gsl::span<const FieldElementT> src, const gsl::span<FieldElementT> dst,
    const MultiplicativeFftBases<FieldElementT>& bases) {
  fft::details::ValidateFFTSizes(src, dst, bases.NumLayers());
  const size_t n = Pow2(bases.NumLayers());
  size_t distance = n;
  gsl::span<const FieldElementT> curr_src = src;

  for (size_t layer_i = 0; layer_i < bases.NumLayers(); ++layer_i) {
    distance >>= 1;
    size_t j = 0;
    auto fft_domain = bases[bases.NumLayers() - 1 - layer_i];
    for (const FieldElementT& x : fft_domain.RemoveFirstBasisElements(1)) {
      for (size_t i = 0; i < distance; i++) {
        const size_t idx = i + j;
        // Note that starting from the second iteration, src == dst so we must use temporary
        // variables.
        FieldElementT tmp = curr_src[idx];
        FieldElementT mul_res = x * curr_src[idx + distance];

        dst[idx] = tmp + mul_res;
        dst[idx + distance] = tmp - mul_res;
      }
      j += 2 * distance;
    }
    // First fft itertion copies the data, the following iteration work in-place.
    curr_src = dst;
  }
}

template <typename BasesT>
void MultiplicativeIfft(
    const BasesT& bases, gsl::span<const typename BasesT::FieldElementT> src,
    gsl::span<typename BasesT::FieldElementT> dst, int n_layers) {
  using FieldElementT = typename BasesT::FieldElementT;
  static const auto kOrder = BasesT::kOrder;
  static_assert(
      std::is_same_v<BasesT, MultiplicativeFftBases<FieldElementT, kOrder>>, "Wrong FftBases");
  ASSERT_RELEASE(
      (n_layers <= static_cast<int>(bases.NumLayers())) && (n_layers >= -1),
      "Wrong number of layers");
  fft::details::ValidateFFTSizes(src, dst, bases.NumLayers());
  if (n_layers == 0 || bases.NumLayers() == 0) {
    std::copy(src.begin(), src.end(), dst.begin());
    return;
  }
  if (n_layers == -1) {
    n_layers = bases.NumLayers();
  }

  if (kOrder == MultiplicativeGroupOrdering::kNaturalOrder) {
    MultiplicativeIfftNatural(bases, src, dst, n_layers);
  } else {
    MultiplicativeIfftReversed(bases, src, dst, n_layers);
  }
}

template <typename BasesT>
void MultiplicativeIfftNatural(
    const BasesT& bases, gsl::span<const typename BasesT::FieldElementT> src,
    gsl::span<typename BasesT::FieldElementT> dst, int n_layers) {
  using FieldElementT = typename BasesT::FieldElementT;

  const size_t n = src.size();
  size_t distance = n;
  gsl::span<const FieldElementT> curr_src = src;

  for (int k = 0; k < n_layers; k++) {
    distance >>= 1;
    const auto& fft_domain_quotient_inverse = bases[k].RemoveLastBasisElements(1).Inverse();

    for (size_t i = 0; i < n; i += 2 * distance) {
      size_t idx = i;

      for (const FieldElementT& x : fft_domain_quotient_inverse) {
        // Note that starting from the second iteration, src == dst so we must use temporary
        // variables.
        BasesT::GroupT::IfftButterfly(
            curr_src[idx], curr_src[idx + distance], x, &dst[idx], &dst[idx + distance]);
        idx++;
      }
    }
    // First fft itertion copies the data, the following iteration work in-place.
    curr_src = dst;
  }
}

template <typename BasesT>
void MultiplicativeIfftReversed(
    const BasesT& bases, gsl::span<const typename BasesT::FieldElementT> src,
    gsl::span<typename BasesT::FieldElementT> dst, int n_layers) {
  using FieldElementT = typename BasesT::FieldElementT;

  size_t distance = 1;
  gsl::span<const FieldElementT> curr_src = src;

  size_t min_log_n_ifft_task_size = 12;

  for (int k = 0; k < n_layers; k++) {
    // Make sure we create tasks that are no shorter than a minimum size unless the domain
    // size is already the size of the minimum task size or smaller, in which case we won't split
    // it.
    const size_t log_n_ifft_tasks = static_cast<size_t>(
        std::max<int64_t>(bases[k].BasisSize() - 1 - min_log_n_ifft_task_size, 0));

    const auto basis_pair = bases[k].RemoveFirstBasisElements(1).Inverse().Split(log_n_ifft_tasks);

    const auto& inner_domain = std::get<0>(basis_pair);
    const auto& outer_domain = std::get<1>(basis_pair);

    TaskManager::GetInstance().ParallelFor(
        outer_domain.Size(),
        [&outer_domain, &inner_domain, distance, curr_src, dst](const TaskInfo& task_info) {
          int64_t task_idx = task_info.start_idx;
          size_t chunk_offset = task_idx * inner_domain.Size() * 2 * distance;

          // Following a domain split the inner_domain has unit offset.
          auto domain = inner_domain.GetShiftedDomain(outer_domain[task_idx]);

          for (const FieldElementT& x : domain) {
            for (size_t i = 0; i < distance; ++i) {
              const size_t idx = chunk_offset + i;

              BasesT::GroupT::IfftButterfly(
                  curr_src[idx], curr_src[idx + distance], x, &dst[idx], &dst[idx + distance]);
            }
            chunk_offset += 2 * distance;  // Move to next chunk.
          }
        });
    // First fft itertion copies the data, the following iteration work in-place.
    curr_src = dst;
    distance <<= 1;
  }
}

}  // namespace starkware
