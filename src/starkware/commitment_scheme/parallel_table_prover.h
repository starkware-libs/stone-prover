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

#ifndef STARKWARE_COMMITMENT_SCHEME_PARALLEL_TABLE_PROVER_H_
#define STARKWARE_COMMITMENT_SCHEME_PARALLEL_TABLE_PROVER_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

class ParallelTableProver : public TableProver {
 public:
  /*
    The ParallelTableProver takes advantage of hardware concurrency and caching by breaking
    each segment passed to it into n_tasks_per_segment_ subsegments and commiting on the
    subsegments concurrently.
    To this end, The ParallelTableProver's segment is n_tasks_per_segment_ times larger
    then the segment size of the internal TableProve.
  */
  ParallelTableProver(
      MaybeOwnedPtr<TableProver> table_prover, size_t n_tasks_per_segment, size_t sub_segment_size)
      : table_prover_(std::move(table_prover)),
        n_tasks_per_segment_(n_tasks_per_segment),
        sub_segment_size_(sub_segment_size) {}

  void AddSegmentForCommitment(
      const std::vector<ConstFieldElementSpan>& segment, size_t segment_index,
      size_t n_interleaved_columns) override;

  void Commit() override;

  std::vector<uint64_t> StartDecommitmentPhase(
      const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) override;

  void Decommit(gsl::span<const ConstFieldElementSpan> elements_data) override;

 private:
  MaybeOwnedPtr<TableProver> table_prover_;
  size_t n_tasks_per_segment_;
  uint64_t sub_segment_size_;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_PARALLEL_TABLE_PROVER_H_
