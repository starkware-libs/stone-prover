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

#include "starkware/utils/bit_reversal.h"

#include "starkware/algebra/utils/invoke_template_version.h"

namespace starkware {

void BitReverseInPlace(const FieldElementSpan& arr) {
  InvokeFieldTemplateVersion(
      [&](auto field_tag) {
        using FieldElementT = typename decltype(field_tag)::type;
        BitReverseInPlace(arr.As<FieldElementT>());
      },
      arr.GetField());
}

void BitReverseVector(const ConstFieldElementSpan& src, const FieldElementSpan& dst) {
  ASSERT_RELEASE(src.Size() == dst.Size(), "Span size must be the same");
  InvokeFieldTemplateVersion(
      [&](auto field_tag) {
        using FieldElementT = typename decltype(field_tag)::type;
        const auto& src_arr = src.As<FieldElementT>();
        const auto& dst_arr = dst.As<FieldElementT>();

        const int logn = SafeLog2(src_arr.size());
        const size_t min_work_chunk = 1024;

        TaskManager& task_manager = TaskManager::GetInstance();

        task_manager.ParallelFor(
            src_arr.size(),
            [src_arr, dst_arr, logn](const TaskInfo& task_info) {
              for (size_t k = task_info.start_idx; k < task_info.end_idx; ++k) {
                const size_t rk = BitReverse(k, logn);
                dst_arr[rk] = src_arr[k];
              }
            },
            src_arr.size(), min_work_chunk);
      },
      src.GetField());
}

}  // namespace starkware
