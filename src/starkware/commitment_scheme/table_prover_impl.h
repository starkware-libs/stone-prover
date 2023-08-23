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

#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_IMPL_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "starkware/channel/prover_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

class TableProverImpl : public TableProver {
 public:
  TableProverImpl(
      size_t n_columns, MaybeOwnedPtr<CommitmentSchemeProver> commitment_scheme,
      ProverChannel* channel)
      : n_columns_(n_columns),
        commitment_scheme_(std::move(commitment_scheme)),
        channel_(channel) {}

  void AddSegmentForCommitment(
      const std::vector<ConstFieldElementSpan>& segment, size_t segment_index,
      size_t n_interleaved_columns) override;

  void Commit() override;

  std::vector<uint64_t> StartDecommitmentPhase(
      const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) override;

  void Decommit(gsl::span<const ConstFieldElementSpan> elements_data) override;

 private:
  size_t n_columns_;
  MaybeOwnedPtr<CommitmentSchemeProver> commitment_scheme_;
  ProverChannel* channel_;
  std::set<RowCol> data_queries_;
  std::set<RowCol> integrity_queries_;
  std::set<uint64_t> all_query_rows_;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_IMPL_H_
