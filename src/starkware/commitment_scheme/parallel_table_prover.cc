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

#include "starkware/commitment_scheme/parallel_table_prover.h"

#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/task_manager.h"

namespace starkware {

void ParallelTableProver::AddSegmentForCommitment(
    const std::vector<ConstFieldElementSpan>& segment, size_t segment_index,
    size_t n_interleaved_columns) {
  TaskManager& task_manager = TaskManager::GetInstance();

  size_t thread_count = task_manager.GetNumThreads();
  std::vector<std::vector<ConstFieldElementSpan>> per_thread_segments(thread_count);

  size_t n_field_elements_in_sub_segment = sub_segment_size_ * n_interleaved_columns;
  task_manager.ParallelFor(
      n_tasks_per_segment_,
      [this, &per_thread_segments, &segment, segment_index, n_field_elements_in_sub_segment,
       n_interleaved_columns](const TaskInfo& task_info) {
        size_t task_idx = task_info.start_idx;
        size_t idx = n_tasks_per_segment_ * segment_index + task_idx;
        auto& sub_segments = per_thread_segments[TaskManager::GetWorkerId()];
        sub_segments.clear();
        for (auto&& column : segment) {
          sub_segments.push_back(column.SubSpan(
              task_idx * n_field_elements_in_sub_segment, n_field_elements_in_sub_segment));
        }

        table_prover_->AddSegmentForCommitment(sub_segments, idx, n_interleaved_columns);
      });
}

void ParallelTableProver::Commit() { table_prover_->Commit(); }

std::vector<uint64_t> ParallelTableProver::StartDecommitmentPhase(
    const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) {
  return table_prover_->StartDecommitmentPhase(data_queries, integrity_queries);
}

void ParallelTableProver::Decommit(gsl::span<const ConstFieldElementSpan> elements_data) {
  table_prover_->Decommit(elements_data);
}

}  // namespace starkware
