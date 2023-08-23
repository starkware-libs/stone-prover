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

#ifndef STARKWARE_ALGEBRA_FFT_TRANSPOSE_H_
#define STARKWARE_ALGEBRA_FFT_TRANSPOSE_H_

#include <algorithm>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/stl_utils/containers.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

/*
  Performs transpose on a block of a n^2 size matrix of FieldElements represented as an array of
  length n^2. The block starts from (corner_i,corner_j) and ends at (corner_i+block_size,
  corner_j+block_size).
*/
template <typename FieldElementT>
static inline void BlockTranspose(
    const gsl::span<FieldElementT> a, size_t n, size_t block_size, size_t corner_i,
    size_t corner_j) {
  if (corner_i != corner_j) {
    // Case 1: Off the diagonal.
    for (size_t i = corner_i; i < corner_i + block_size; ++i) {
      for (size_t j = corner_j; j < corner_j + block_size; j++) {
        std::swap(UncheckedAt(a, j * n + i), UncheckedAt(a, i * n + j));
      }
    }
  } else {
    // Case 2: On the diagonal.
    for (size_t i = corner_i; i < corner_i + block_size; ++i) {
      for (size_t j = corner_j; j < i; j++) {
        std::swap(UncheckedAt(a, j * n + i), UncheckedAt(a, i * n + j));
      }
    }
  }
}

/*
  Performs transpose on a n^2 size matrix of FieldElements represented as an array of length n^2.
*/
template <typename FieldElementT>
static inline void Transpose(const gsl::span<FieldElementT> a) {
  BlockTranspose(a, a.size(), a.size(), 0, 0);
}

/*
  Performs transpose on a n^2 size matrix of FieldElements represented as an array of length n^2.
  Uses parallelization to transpose small blocks of the matrix (max size of block is 16x16).
*/
template <typename FieldElementT>
static inline void ParallelTranspose(const gsl::span<FieldElementT> a, size_t n) {
  size_t block_size = std::min<size_t>(n, 16);
  std::vector<std::pair<size_t, size_t>> transpose_plan;
  for (size_t i = 0; i < n; i += block_size) {
    for (size_t j = 0; j <= i; j += block_size) {
      transpose_plan.emplace_back(i, j);
    }
  }
  auto f = [&transpose_plan, a, n, block_size](const TaskInfo& task_info) {
    auto& [i, j] = transpose_plan[task_info.start_idx];
    BlockTranspose(a, n, block_size, i, j);
  };
  TaskManager::GetInstance().ParallelFor(0U, transpose_plan.size(), f);
}

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_FFT_TRANSPOSE_H_
