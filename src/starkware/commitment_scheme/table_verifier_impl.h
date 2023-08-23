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

#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_IMPL_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

class TableVerifierImpl : public TableVerifier {
 public:
  TableVerifierImpl(
      Field field, size_t n_columns, MaybeOwnedPtr<CommitmentSchemeVerifier> commitment_scheme,
      VerifierChannel* channel)
      : field_(std::move(field)),
        n_columns_(n_columns),
        commitment_scheme_(std::move(commitment_scheme)),
        channel_(channel) {}

  void ReadCommitment() override;

  std::map<RowCol, FieldElement> Query(
      const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) override;

  bool VerifyDecommitment(const std::map<RowCol, FieldElement>& all_rows_data) override;

 private:
  Field field_;
  size_t n_columns_;
  MaybeOwnedPtr<CommitmentSchemeVerifier> commitment_scheme_;
  VerifierChannel* channel_;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_IMPL_H_
