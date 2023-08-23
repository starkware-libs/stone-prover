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


#include "third_party/cppitertools/range.hpp"

#include "starkware/algebra/fft/fft_with_precompute.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/fft_utils/fft_bases.h"
#include "starkware/math/math.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

namespace fft {
namespace details {

template <typename FieldElementT>
static inline void ValidateFFTSizes(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst, size_t log_n) {
  const size_t n = Pow2(log_n);
  ASSERT_RELEASE(src.size() == n, "src must be of size 2^log_n.");
  ASSERT_RELEASE(dst.size() == n, "dst must be of size 2^log_n.");
}

inline size_t FftNumPrecomputedTwiddleFactors(size_t precompute_depth) {
  // The number of TwiddleFactors is 1+2+4+...+2^(Precompute_depth -1) = 2^Precompute_depth - 1.
  return Pow2(precompute_depth) - 1;
}

template <typename FieldElementT>
void NormalizeArray(gsl::span<FieldElementT> buff) {
  for (size_t idx = 0; idx < buff.size(); idx++) {
    FieldElementT::FftNormalize(&UncheckedAt(buff, idx));
  }
}

// Shifts twiddle factors by shift_element.
template <typename FieldElementT, typename BasesT>
void ComputeTwiddleFromOtherTwiddle(
    FieldElementT shift_element, const BasesT& bases, size_t initial_layer,
    gsl::span<FieldElementT> factors_src, gsl::span<FieldElementT> factors_dst) {
  const size_t n = factors_src.size();
  size_t layer_size = (n + 1) >> 1;
  ASSERT_RELEASE(
      IsPowerOfTwo(factors_src.size() + 1), "Twiddle length should be 2^m-1 for some m.");
  ASSERT_RELEASE(
      factors_src.size() == factors_dst.size(), "src and dst must be of the same length.");
  size_t layer_index = 0;
  size_t layer_end = n - 1;
  while (layer_size > 0) {
    // Writes are going backward to make the writes continuous.
    for (size_t i = 0; i < layer_size; ++i) {
      factors_dst[layer_end - i] =
          BasesT::GroupT::GroupOperation(factors_src[layer_end - i], shift_element);
    }
    shift_element = bases.ApplyBasisTransformTmpl(shift_element, initial_layer + layer_index);
    layer_end -= layer_size;
    layer_size >>= 1;
    layer_index += 1;
  }
}

// Shifts one layer of twiddle factors by new_coset_offset.
template <typename FieldElementT, typename GroupT>
void ComputeTwiddleFromOtherTwiddleConst(
    const FieldElementT shift_constant, gsl::span<FieldElementT> factors_out) {
  ASSERT_RELEASE(IsPowerOfTwo(factors_out.size()), "not a power of two.");
  for (size_t i = 0; i < factors_out.size(); ++i) {
    factors_out[i] = GroupT::GroupOperation(factors_out[i], shift_constant);
  }
}

template <typename FieldElementT>
void FftPrecomputeNaturalOrderOneLayer(
    FieldElementT offset, FieldElementT generator, size_t layer_size,
    gsl::span<FieldElementT> factors_out) {
  auto it = factors_out.begin();
  for (size_t i = 0; i < layer_size; ++i) {
    *it = offset;
    ++it;
    if (i < layer_size - 1) {
      offset *= generator;
    }
  }
}

/*
  In the Multiplicative natural order case, we can calculate the TwiddleFactors without using
  the stack based iterator of fft_domain.
*/
template <typename BasesT>
void FftPrecomputeNaturalOrderMultipleLayers(
    const BasesT& bases, std::vector<typename BasesT::FieldElementT> generators, size_t depth,
    gsl::span<typename BasesT::FieldElementT> factors_out) {
  size_t distance = 1;
  auto num_fft_layers = bases.NumLayers();
  size_t index = 0;
  for (size_t i = 0; i < depth; ++i) {
    const size_t layer = num_fft_layers - i;
    FftPrecomputeNaturalOrderOneLayer(
        bases[layer - 1].StartOffset(), generators[layer - 1], distance,
        factors_out.subspan(index, distance));
    index += distance;
    distance <<= 1;
  }
}

/*
  In the Multiplicative Natural Order case, we rearrange the twiddle factors so that in four step
  FFT, each task in the ParallelFor will have contiguous data.
  The twiddle factors size is n-1.

  Assuming n is a square.
  The first sqrt_n twiddle factors are for the group c^sqrt_n*<g^sqrt_n>.
  Then, for 0 <= i < sqrt_n we have the twiddle factors for c*g^i*<g^sqrt_n>.
  Where c = bases[0].StartOffset() and g = bases[0].Basis()[0].

  If n is not a square, the resulting twiddle factors are going to be split into two parts:
  The first half (n/2) will be as mentioned above for n/2 (which is a square) but with offset c^2
  (note that the generator for the subgroup of order n/2 is g^2).
  The second half (n/2 + 1) will be the twiddle factors of the last layer, in regular order.
*/
template <typename BasesT>
void FftPrecomputeFourStepNaturalOrderTwiddleFactors(
    const BasesT& bases, size_t precompute_depth,
    gsl::span<typename BasesT::FieldElementT> factors_out) {
  using FieldElementT = typename BasesT::FieldElementT;

  const auto num_fft_layers = bases.NumLayers();
  ASSERT_RELEASE(precompute_depth <= num_fft_layers, "precompute_depth > num_fft_layers.");
  ASSERT_RELEASE(
      factors_out.size() <= FftNumPrecomputedTwiddleFactors(precompute_depth),
      "factors_out is to small.");

  const auto& sub_groups_generators = bases[0].Basis();
  if (num_fft_layers < FLAGS_four_step_fft_threshold) {
    // Return twiddle factors in regular order.
    FftPrecomputeNaturalOrderMultipleLayers<BasesT>(
        bases, sub_groups_generators, precompute_depth, factors_out);
    return;
  }
  const size_t initial_depth = num_fft_layers / 2;
  size_t num_tasks = Pow2(initial_depth);
  // Create base twiddle_factors without shift which will be used by all tasks.
  size_t twiddle_size = num_tasks - 1;
  std::vector<FieldElementT> initial_factors = FieldElementT::UninitializedVector(twiddle_size);
  FftPrecomputeNaturalOrderMultipleLayers<BasesT>(
      bases.GetShiftedBases(FieldElementT::One()), sub_groups_generators, initial_depth,
      initial_factors);
  FieldElementT c = bases[0].StartOffset();
  FieldElementT c_sqrt_n = bases[DivCeil(num_fft_layers, 2)].StartOffset();
  auto g = sub_groups_generators[0];
  std::vector<FieldElementT> offsets = FieldElementT::UninitializedVector(num_tasks);
  if (num_fft_layers % 2 == 1) {
    // If there is an odd number of layers, the last layer has to be ordered regularly.
    size_t last_layer_size = Pow2(num_fft_layers - 1);
    FftPrecomputeNaturalOrderOneLayer(
        bases[0].StartOffset(), g, last_layer_size,
        factors_out.subspan(last_layer_size - 1, last_layer_size));
    // Need to update initial offset and generator for the rest of the layers.
    g *= g;
    c *= c;
  }
  // Calclate offsets needed for each task.
  for (size_t i = 0; i < num_tasks; ++i) {
    offsets[i] = c;
    c *= g;
  }
  TaskManager& task_manager = TaskManager::GetInstance();
  task_manager.ParallelFor(
      num_tasks + 1, [c_sqrt_n, num_fft_layers, factors_out, twiddle_size, &initial_factors, &bases,
                      &offsets](const TaskInfo& task_info) {
        if (task_info.start_idx == 0) {
          // Need to multiply the general one created by the shift constant.
          ComputeTwiddleFromOtherTwiddle<FieldElementT>(
              c_sqrt_n, bases, DivCeil(num_fft_layers, 2), initial_factors,
              factors_out.subspan(0, twiddle_size));
          return;
        }
        auto offset = offsets[task_info.start_idx - 1];
        // offset is c*g^task_num.
        ComputeTwiddleFromOtherTwiddle<FieldElementT>(
            offset, bases, 1, initial_factors,
            factors_out.subspan(twiddle_size * task_info.start_idx, twiddle_size));
      });
}

/*
  Precomputes all the twiddle Factor spanned by bases up to precompute_depth.
*/
template <typename BasesT>
void FftPrecomputeTwiddleFactors(
    const BasesT& bases, size_t precompute_depth,
    gsl::span<typename BasesT::FieldElementT> factors_out) {
  const auto num_layers = bases.NumLayers();
  ASSERT_RELEASE(precompute_depth <= num_layers, "precompute_depth > num_layers.");
  ASSERT_RELEASE(
      factors_out.size() <= FftNumPrecomputedTwiddleFactors(precompute_depth),
      "factors_out is to small.");
  auto it = factors_out.begin();

  for (size_t i = 0; i < precompute_depth; ++i) {
    const size_t layer = bases.NumLayers() - i - 1;
    for (const auto& factor : bases[layer].RemoveFirstBasisElements(1)) {
      *it = factor;
      ++it;
    }
  }
}

/*
  Precomputes all the Twiddle Factor required to calculate an fft of size 2^offset_squares.size()
  The result is in factors_out.
*/
template <typename BasesT>
std::vector<typename BasesT::FieldElementT> FftPrecomputeTwiddleFactors(
    const BasesT& bases, size_t precompute_depth) {
  using FieldElementT = typename BasesT::FieldElementT;
  precompute_depth = bases.NumLayers();

  std::vector<FieldElementT> factors_out =
      FieldElementT::UninitializedVector(FftNumPrecomputedTwiddleFactors(precompute_depth));

  if constexpr (BasesT::kOrder == MultiplicativeGroupOrdering::kNaturalOrder) {  // NOLINT
    FftPrecomputeFourStepNaturalOrderTwiddleFactors(bases, precompute_depth, factors_out);
  } else {  // NOLINT
    FftPrecomputeTwiddleFactors(bases, precompute_depth, factors_out);
  }

  return factors_out;
}

template <typename FieldElementT, typename BasesT>
void ParallelFromOtherTwiddle(
    FieldElementT new_coset_offset, const BasesT& bases, gsl::span<FieldElementT> factors_out) {
  const size_t num_fft_layers = bases.NumLayers();
  ASSERT_RELEASE(
      factors_out.size() == Pow2(num_fft_layers) - 1, "Length of factors_out should be 2^k-1.");
  if (num_fft_layers == 0) {
    return;
  }

  TaskManager& task_manager = TaskManager::GetInstance();

  if (BasesT::kOrder == MultiplicativeGroupOrdering::kNaturalOrder &&
      num_fft_layers >= FLAGS_four_step_fft_threshold) {
    // Need to handle the twiddle shifting differently for NaturalOrder twiddle factors.
    const size_t sqrt_n = Pow2(num_fft_layers / 2);
    const size_t twiddle_size = sqrt_n - 1;
    FieldElementT c = new_coset_offset;
    if (num_fft_layers % 2 == 1) {
      size_t last_layer_size = Pow2(num_fft_layers - 1);
      ComputeTwiddleFromOtherTwiddleConst<FieldElementT, typename BasesT::GroupT>(
          c, factors_out.subspan(last_layer_size - 1, last_layer_size));
      c = c * c;
    }

    FieldElementT c_sqrt_n = Pow(c, sqrt_n);
    task_manager.ParallelFor(
        sqrt_n + 1, [&c, &c_sqrt_n, twiddle_size, &factors_out, &bases](const TaskInfo& task_info) {
          const auto& offset = (task_info.start_idx == 0) ? c_sqrt_n : c;
          ComputeTwiddleFromOtherTwiddle<FieldElementT, BasesT>(
              offset, bases, 0,
              factors_out.subspan(twiddle_size * task_info.start_idx, twiddle_size),
              factors_out.subspan(twiddle_size * task_info.start_idx, twiddle_size));
        });

    return;
  }

  // Only a power of 2 num_tasks and chunk_size are supported.
  // The first task works on the first (chunk_size - 1) twiddle factors.
  // The rest of the tasks work on a chunk of size chunk_size.
  // This work division simplifies the implementation as only the first task has to work on twiddle
  // factors from different fft layers.
  int64_t num_tasks = Pow2(Log2Ceil(task_manager.GetNumThreads()));

  // factors_out.size() = 2^num_fft_layers - 1, round it up to ease the calculation.
  const int64_t rounded_twiddle_count = factors_out.size() + 1;
  int64_t chunk_size = rounded_twiddle_count / num_tasks;
  int64_t min_chunk_size = Pow2(FLAGS_log_min_twiddle_shift_task_size);
  if (chunk_size < min_chunk_size) {
    chunk_size = std::min(min_chunk_size, rounded_twiddle_count);
    num_tasks = rounded_twiddle_count / chunk_size;
  }
  const size_t n_required_coset_offsets = SafeLog2(num_tasks) + 1;
  std::vector<FieldElementT> coset_offsets;
  coset_offsets.reserve(n_required_coset_offsets);
  coset_offsets.push_back(new_coset_offset);
  for (size_t i = 1; i < n_required_coset_offsets; ++i) {
    coset_offsets.push_back(bases.ApplyBasisTransformTmpl(coset_offsets.back(), i - 1));
  }

  task_manager.ParallelFor(
      num_tasks, [num_fft_layers, chunk_size, &coset_offsets, &factors_out,
                  &bases](const TaskInfo& task_info) {
        int64_t start_index = task_info.start_idx * chunk_size - 1;
        int64_t end_index = start_index + chunk_size;
        size_t curr_layer = num_fft_layers - 1 - Log2Floor(end_index);
        if (start_index < 0) {
          // Only the first task does this flow. The rest of the tasks execute the other flow.
          ComputeTwiddleFromOtherTwiddle<FieldElementT, BasesT>(
              coset_offsets[curr_layer], bases, curr_layer, factors_out.subspan(0, chunk_size - 1),
              factors_out.subspan(0, chunk_size - 1));
          return;
        }
        // This is part of a single layer of the twiddle factors, therefore the shifting is
        // constant throught this call.
        ComputeTwiddleFromOtherTwiddleConst<FieldElementT, typename BasesT::GroupT>(
            coset_offsets[curr_layer], factors_out.subspan(start_index, chunk_size));
      });
}

/*
  The following function uses pointers rather than spans because using spans
  causes a performance regression with LongField.
*/
template <typename FieldElementT>
ALWAYS_INLINE void FftNaturalToReverseLoop(
    const FieldElementT* src, size_t length, const FieldElementT* twiddle_factors, size_t distance,
    FieldElementT* dst) {
  size_t twiddle_index = 0;

  for (size_t i = 0; i < length; i += 2 * distance) {
    // NOLINTNEXTLINE: do not use pointer arithmetic.
    const auto& twiddle_factor = twiddle_factors[twiddle_index];
    twiddle_index++;
    for (size_t j = 0; j < distance; j++) {
      size_t idx = i + j;
      FieldElementT::FftButterfly(  // NOLINTNEXTLINE: do not use pointer arithmetic.
          src[idx], src[idx + distance], twiddle_factor, &dst[idx], &dst[idx + distance]);
    }
  }
}

#ifndef __EMSCRIPTEN__

template <>
inline void FftNaturalToReverseLoop<PrimeFieldElement<252, 0>>(
    const PrimeFieldElement<252, 0>* src, size_t length,
    const PrimeFieldElement<252, 0>* twiddle_factors, uint64_t distance,
    PrimeFieldElement<252, 0>* dst) {
  // Note that when distance == 1 we use n/2 twiddle factors.
  uint64_t twiddle_shift = 1 + SafeLog2(distance);

  static_assert(IsPowerOfTwo(sizeof(PrimeFieldElement<252, 0>)));
  // In the NaturalToReverse case we only need to mask out the lsb bits.
  uint64_t aligned_twiddle_mask = ~static_cast<uint64_t>((sizeof(PrimeFieldElement<252, 0>) - 1));

  // NOLINTNEXTLINE: do not use pointer arithmetic.
  const PrimeFieldElement<252, 0>* src_plus_distance = src + distance;
  // NOLINTNEXTLINE: do not use pointer arithmetic.
  const PrimeFieldElement<252, 0>* src_end = src + length;

  uint64_t src_to_dst = (dst - src) * sizeof(PrimeFieldElement<252, 0>);
  uint64_t distance_in_bytes = distance * sizeof(PrimeFieldElement<252, 0>);

  Prime0FftLoop(
      src_plus_distance, src_end, src_to_dst, distance_in_bytes, twiddle_factors, twiddle_shift,
      aligned_twiddle_mask);
}
#endif

template <typename FieldElementT>
void FftUsingPrecomputedTwiddleFactorsInner(
    gsl::span<const FieldElementT> src, gsl::span<const FieldElementT> twiddle_factors,
    size_t layers_to_skip, size_t iterations, bool normalize, gsl::span<FieldElementT> dst,
    size_t twiddle_tree_root_index, size_t twiddle_stride) {
  const size_t n = src.size();
  size_t jump = twiddle_stride;

  size_t distance = Pow2(layers_to_skip);
  gsl::span<const FieldElementT> curr_src = src;

  for (size_t layer = 0; layer < iterations; layer++) {
    for (size_t i = 0; i < n; i += 2 * distance) {
      for (size_t j = 0; j < distance; j++) {
        size_t idx = i + j;
        FieldElementT::FftButterfly(
            UncheckedAt(curr_src, idx), UncheckedAt(curr_src, idx + distance),
            UncheckedAt(twiddle_factors, twiddle_tree_root_index + j * twiddle_stride),
            &UncheckedAt(dst, idx), &UncheckedAt(dst, idx + distance));
      }
    }

    twiddle_tree_root_index += jump;
    jump *= 2;
    // First fft iteration copies the data, the following iteration work in-place.
    curr_src = dst;
    distance <<= 1;
  }

  if (normalize) {
    NormalizeArray(dst);
  }
}

template <typename FieldElementT>
void ButterflyTwoArraysNatural(
    gsl::span<const FieldElementT> src_a, gsl::span<const FieldElementT> src_b,
    gsl::span<FieldElementT> dst_a, gsl::span<FieldElementT> dst_b,
    gsl::span<const FieldElementT> twiddle_factors) {
  size_t n = src_a.size();
  for (size_t idx = 0; idx < n; idx++) {
    FieldElementT::FftButterfly(
        UncheckedAt(src_a, idx), UncheckedAt(src_b, idx), UncheckedAt(twiddle_factors, idx),
        &UncheckedAt(dst_a, idx), &UncheckedAt(dst_b, idx));
  }
}

template <typename FieldElementT>
void ParallelButterflyTwoArraysNatural(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    gsl::span<const FieldElementT> twiddle_factors, bool normalize,
    const size_t max_chunk_size = 256) {
  size_t distance = src.size() / 2;
  size_t last_twiddle_layer_idx = twiddle_factors.size() / 2;
  auto twiddle_factors_half = twiddle_factors.subspan(last_twiddle_layer_idx);
  TaskManager& task_manager = TaskManager::GetInstance();
  const size_t chunk = std::min<size_t>(max_chunk_size, distance);
  task_manager.ParallelFor(
      distance / chunk,
      [twiddle_factors_half, chunk, distance, src, dst, normalize](const TaskInfo& task_info) {
        size_t task_start_idx = task_info.start_idx * chunk;
        ButterflyTwoArraysNatural<FieldElementT>(
            src.subspan(task_start_idx, chunk), src.subspan(task_start_idx + distance, chunk),
            dst.subspan(task_start_idx, chunk), dst.subspan(task_start_idx + distance, chunk),
            twiddle_factors_half.subspan(task_start_idx, chunk));
        if (normalize) {
          NormalizeArray(dst.subspan(task_start_idx, chunk));
          NormalizeArray(dst.subspan(task_start_idx + distance, chunk));
        }
      });
}

template <typename FieldElementT>
void FourStepFftNatural(
    gsl::span<const FieldElementT> twiddle_factors, gsl::span<FieldElementT> buff,
    size_t twiddle_factor_root_index, size_t initial_num_of_layers, bool normalize) {
  const size_t num_tasks = Pow2(initial_num_of_layers);
  const size_t chunk = num_tasks;

  TaskManager& task_manager = TaskManager::GetInstance();
  task_manager.ParallelFor(
      num_tasks, [chunk, initial_num_of_layers, twiddle_factor_root_index, twiddle_factors,
                  buff](const TaskInfo& task_info) {
        uint64_t task_start_idx = task_info.start_idx * chunk;
        FftUsingPrecomputedTwiddleFactorsInner<FieldElementT>(
            buff.subspan(task_start_idx, chunk), twiddle_factors, 0, initial_num_of_layers, false,
            buff.subspan(task_start_idx, chunk), twiddle_factor_root_index, 1);
      });
  ParallelTranspose(buff, num_tasks);

  const size_t twiddle_size = chunk - 1;

  task_manager.ParallelFor(num_tasks, [&](const TaskInfo& task_info) {
    uint64_t work_id = task_info.start_idx;
    FftUsingPrecomputedTwiddleFactorsInner<FieldElementT>(
        buff.subspan(chunk * work_id, chunk), twiddle_factors, 0, initial_num_of_layers, normalize,
        buff.subspan(chunk * work_id, chunk), twiddle_size * (work_id + 1), 1);
  });
  ParallelTranspose(buff, num_tasks);
}

template <typename FieldElementT>
void FftUsingPrecomputedTwiddleFactors(
    gsl::span<const FieldElementT> src, gsl::span<const FieldElementT> twiddle_factors,
    bool normalize, gsl::span<FieldElementT> dst) {
  const size_t n = src.size();
  const size_t num_fft_layers = SafeLog2(n);
  const size_t initial_num_layers = num_fft_layers / 2;

  if (num_fft_layers < FLAGS_four_step_fft_threshold) {
    FftUsingPrecomputedTwiddleFactorsInner<FieldElementT>(
        src, twiddle_factors, 0, num_fft_layers, normalize, dst, 0, 1);
    return;
  }

  ValidateFFTSizes(src, dst, num_fft_layers);
  if (src.data() != dst.data()) {
    std::copy(src.begin(), src.end(), dst.begin());
  }

  size_t twiddle_factor_root_index = 0;
  if (num_fft_layers % 2 == 1) {
    FourStepFftNatural(
        twiddle_factors, dst.subspan(0, n / 2), twiddle_factor_root_index, initial_num_layers,
        false);
    FourStepFftNatural(
        twiddle_factors, dst.subspan(n / 2), twiddle_factor_root_index, initial_num_layers, false);

    ParallelButterflyTwoArraysNatural<FieldElementT>(dst, dst, twiddle_factors, normalize, 256);
  } else {
    FourStepFftNatural(
        twiddle_factors, dst, twiddle_factor_root_index, initial_num_layers, normalize);
  }
}

template <typename BasesT>
void FftNoPrecompute(
    const gsl::span<const typename BasesT::FieldElementT> src, const BasesT& bases,
    size_t layers_to_skip, gsl::span<typename BasesT::FieldElementT> dst) {
  using FieldElementT = typename BasesT::FieldElementT;
  const size_t num_fft_layers = bases.NumLayers();
  ValidateFFTSizes(src, dst, num_fft_layers);
  const size_t n = src.size();

  size_t distance = Pow2(layers_to_skip);
  gsl::span<const FieldElementT> curr_src = src;

  ASSERT_RELEASE(num_fft_layers > layers_to_skip, "Trying to skip too many layers.");
  for (size_t layer = num_fft_layers - layers_to_skip; layer > 1; layer--) {
    FieldElementT offset = bases[layer - 1].StartOffset();
    FieldElementT generator = bases[layer - 1].Basis().front();
    for (size_t i = 0; i < n; i += 2 * distance) {
      FieldElementT x = offset;
      for (size_t j = 0; j < distance; j++) {
        const size_t idx = i + j;
        // Note that starting from the second iteration, src == dst so we must use temporary
        // variables.
        FieldElementT::FftButterfly(
            UncheckedAt(curr_src, idx), UncheckedAt(curr_src, idx + distance), x,
            &UncheckedAt(dst, idx), &UncheckedAt(dst, idx + distance));
        // Skip the multiplication on the last iteration.
        if (j < distance - 1) {
          x *= generator;
        }
      }
    }
    // First fft iteration copies the data, the following iteration work in-place.
    curr_src = dst;
    distance <<= 1;
  }

  FieldElementT x = bases[0].StartOffset();
  FieldElementT generator = bases[0].Basis().front();
  for (size_t idx = 0; idx < distance; idx++) {
    // Here curr_src == dst unless num_fft_layers == 1.
    FieldElementT::FftButterfly(
        UncheckedAt(curr_src, idx), UncheckedAt(curr_src, idx + distance), x,
        &UncheckedAt(dst, idx), &UncheckedAt(dst, idx + distance));

    FieldElementT::FftNormalize(&UncheckedAt(dst, idx));
    FieldElementT::FftNormalize(&UncheckedAt(dst, idx + distance));
    // Skip the multiplication on the last iteration.
    if (idx < distance - 1) {
      x *= generator;
    }
  }
}

template <typename FieldElementT>
void ButterflyTwoArrays(
    gsl::span<const FieldElementT> src_a, gsl::span<const FieldElementT> src_b,
    gsl::span<FieldElementT> dst_a, gsl::span<FieldElementT> dst_b,
    const FieldElementT twiddle_factor) {
  size_t n = src_a.size();
  for (size_t idx = 0; idx < n; idx++) {
    FieldElementT::FftButterfly(
        UncheckedAt(src_a, idx), UncheckedAt(src_b, idx), twiddle_factor, &UncheckedAt(dst_a, idx),
        &UncheckedAt(dst_b, idx));
  }
}

template <typename FieldElementT>
void ParallelButterflyTwoArrays(
    gsl::span<const FieldElementT> src, gsl::span<FieldElementT> dst,
    const FieldElementT twiddle_factor, const size_t max_chunk_size = 256) {
  size_t distance = src.size() / 2;
  TaskManager& task_manager = TaskManager::GetInstance();
  const size_t chunk = std::min<size_t>(max_chunk_size, distance);
  task_manager.ParallelFor(
      distance / chunk, [chunk, distance, src, dst, twiddle_factor](const TaskInfo& task_info) {
        uint64_t task_index = task_info.start_idx;
        ButterflyTwoArrays(
            src.subspan(task_index * chunk, chunk),
            src.subspan(task_index * chunk + distance, chunk),
            dst.subspan(task_index * chunk, chunk),
            dst.subspan(task_index * chunk + distance, chunk), twiddle_factor);
      });
}

/*
  Auxiliary function that computes FFT on an array, dst, of size 2^{2k}.
  Input is in natural order (N), output is in bit reversal order (R).
  The computation method:
    - The function views dst a sqrt(n)xsqrt(n) matrix. It then transposes the matrix, and computes
    sqrt(n) FFTs on the sqrt(n) rows of the transposed matrix, for half of the needed layers.
    - It then transposes the matrix again, and computes sqrt(n) FFTs on the sqrt(n) rows of the
  matrix, for the remaining layers.
*/
template <typename FieldElementT>
void FourStepFft(
    gsl::span<const FieldElementT> twiddle_factors, gsl::span<FieldElementT> buff,
    size_t twiddle_tree_root_index, size_t initial_num_of_layers, bool normalize = true) {
  ASSERT_RELEASE(SafeLog2(buff.size()) % 2 == 0, "buff must be of size 2^{2k}.");
  TaskManager& task_manager = TaskManager::GetInstance();
  const size_t num_tasks = Pow2(initial_num_of_layers);
  const size_t chunk = num_tasks;
  ParallelTranspose(buff, num_tasks);
  task_manager.ParallelFor(
      num_tasks, [buff, &twiddle_factors, chunk, initial_num_of_layers,
                  twiddle_tree_root_index](const TaskInfo& task_info) {
        uint64_t work_id = task_info.start_idx;
        FftNaturalToReverseWithPrecomputeInner<FieldElementT>(
            buff.subspan(chunk * work_id, chunk), twiddle_factors,
            buff.subspan(chunk * work_id, chunk), twiddle_tree_root_index, initial_num_of_layers,
            false);
      });
  ParallelTranspose(buff, num_tasks);
  const size_t twiddle_factors_curr_index = chunk * (twiddle_tree_root_index + 1) - 1;
  // Next, use num_of_threads threads to perform the remaining layers.
  task_manager.ParallelFor(
      num_tasks, [buff, &twiddle_factors, chunk, twiddle_factors_curr_index, initial_num_of_layers,
                  normalize](const TaskInfo& task_info) {
        uint64_t work_id = task_info.start_idx;
        FftNaturalToReverseWithPrecomputeInner<FieldElementT>(
            buff.subspan(chunk * work_id, chunk), twiddle_factors,
            buff.subspan(chunk * work_id, chunk), twiddle_factors_curr_index + work_id,
            initial_num_of_layers, normalize);
      });
}

/*
  Computes FFT NaturalToReverse.
  Input is in natural order (N), output is in bit reversal order (R).
  The computation method:
  If the size of src is 2^{2k+1}:
    - Compute one layer of FFT, and then compute FFT on both halves of src using FourStepFFt.
  If the size of src is 2^{2k}:
    - Compute FFT of src using FourStepFFt.
*/
template <typename FieldElementT>
void FftNaturalToReverseWithPrecompute(
    gsl::span<const FieldElementT> src, gsl::span<const FieldElementT> twiddle_factors,
    gsl::span<FieldElementT> dst, bool normalize) {
  const size_t n = src.size();
  const size_t num_fft_layers = SafeLog2(n);
  ValidateFFTSizes(src, dst, num_fft_layers);
  const size_t initial_num_of_layers = num_fft_layers / 2;
  size_t fft_size = n;
  size_t twiddle_tree_root_index = 0;

  if (num_fft_layers < FLAGS_four_step_fft_threshold) {
    FftNaturalToReverseWithPrecomputeInner<FieldElementT>(
        src, twiddle_factors, dst, 0, num_fft_layers, normalize);
    return;
  }

  if (num_fft_layers % 2 /*If odd*/) {
    ParallelButterflyTwoArrays(src, dst, twiddle_factors[0]);
    fft_size /= 2;
    twiddle_tree_root_index += 1;
  } else if (src.data() != dst.data()) {
    std::copy(src.begin(), src.end(), dst.begin());
  }
  FourStepFft(
      twiddle_factors, dst.subspan(0, fft_size), twiddle_tree_root_index, initial_num_of_layers,
      normalize);
  if (num_fft_layers % 2 /*If odd*/) {
    FourStepFft(
        twiddle_factors, dst.subspan(fft_size), twiddle_tree_root_index + 1, initial_num_of_layers,
        normalize);
  }
}

template <typename FieldElementT>
void FftNaturalToReverseWithPrecomputeInner(
    gsl::span<const FieldElementT> src, gsl::span<const FieldElementT> twiddle_factors,
    gsl::span<FieldElementT> dst, size_t twiddle_tree_root_index, size_t stop_layer,
    bool normalize) {
  const size_t n = src.size();
  const size_t num_fft_layers = SafeLog2(n);
  ValidateFFTSizes(src, dst, num_fft_layers);
  if (num_fft_layers == 0) {
    dst[0] = src[0];
    return;
  }
  gsl::span<const FieldElementT> curr_src = src;

  size_t distance = n;

  for (size_t layer = 0; layer < stop_layer; layer++) {
    distance >>= 1;
    FftNaturalToReverseLoop<FieldElementT>(
        curr_src.data(), n, &UncheckedAt(twiddle_factors, twiddle_tree_root_index), distance,
        dst.data());
    // First fft iteration copies the data, the following iteration work in-place.
    curr_src = dst;
    twiddle_tree_root_index = twiddle_tree_root_index * 2 + 1;
  }
  if (normalize) {
    NormalizeArray(dst);
  }
}

}  // namespace details
}  // namespace fft

}  // namespace starkware
