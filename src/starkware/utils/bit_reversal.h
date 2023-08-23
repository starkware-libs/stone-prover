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

#ifndef STARKWARE_UTILS_BIT_REVERSAL_H_
#define STARKWARE_UTILS_BIT_REVERSAL_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/polymorphic/field_element_span.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

using std::size_t;

/*
  Returns the bit reversal of n assuming it has the given number of bits.
  for example if we have number_of_bits = 6 and n = (0b)1101 == (0b)001101
  the function will return (0b)101100.
*/
static inline uint64_t BitReverse(uint64_t n, const size_t number_of_bits) {
  if (number_of_bits == 0) {
    // Avoid undefined behavior when shifting by 64 - number_of_bits.
    return n;
  }

  ASSERT_DEBUG(
      number_of_bits == 64 || n < Pow2(number_of_bits), "n must be smaller than 2^number_of_bits");

  // Bit reverse all the bytes.
  std::array<uint64_t, 3> masks{0x5555555555555555, 0x3333333333333333, 0xf0f0f0f0f0f0f0f};
  size_t bits_to_swap = 1;
  for (auto mask : masks) {
    n = ((n & mask) << bits_to_swap) | ((n >> bits_to_swap) & mask);
    bits_to_swap <<= 1;
  }

  // Swap all the bytes using Built-in Functions, should result in 1 asm instruction.
  n = __builtin_bswap64(n);

  // Right adjust the result according to number_of_bits.
  return n >> (64 - number_of_bits);
}

/*
  Applies the bit reversal permutation of the input span.
  The input size needs to be Pow2(logn).
  The result will satify:
  new_input[i] = input[(BitReverse(i, logn)] for each i in [0, Pow2(logn)).
*/
template <typename T>
void BitReverseInPlace(gsl::span<T> arr) {
  const int logn = SafeLog2(arr.size());
  const size_t min_work_chunk = 1024;

  TaskManager& task_manager = TaskManager::GetInstance();

  task_manager.ParallelFor(
      arr.size(),
      [arr, logn](const TaskInfo& task_info) {
        for (size_t k = task_info.start_idx; k < task_info.end_idx; ++k) {
          const size_t rk = BitReverse(k, logn);
          if (k < rk) {
            std::swap(arr[k], arr[rk]);
          }
        }
      },
      arr.size(), min_work_chunk);
}

void BitReverseInPlace(const FieldElementSpan& arr);

/*
  Same as BitReverseVector(), but works on vector and returns the result instead of changing the
  vector.
*/
template <typename T>
std::vector<T> BitReverseVector(std::vector<T> vec) {
  BitReverseInPlace(gsl::make_span(vec));
  return vec;
}

void BitReverseVector(const ConstFieldElementSpan& src, const FieldElementSpan& dst);

}  // namespace starkware

#endif  // STARKWARE_UTILS_BIT_REVERSAL_H_
