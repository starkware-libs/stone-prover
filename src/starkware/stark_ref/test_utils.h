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

#ifndef STARKWARE_STARK_TEST_UTILS_H_
#define STARKWARE_STARK_TEST_UTILS_H_

#include <memory>
#include <utility>

#include "starkware/algebra/field_operations.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"

namespace starkware {

template <typename HashT, typename FieldElementT>
std::unique_ptr<TableVerifier> MakeTableVerifier(
    const Field& field, uint64_t n_rows, uint64_t n_columns, VerifierChannel* channel,
    size_t n_verifier_friendly_commitment_layers = 0,
    CommitmentHashes commitment_hashes = CommitmentHashes(HashT::HashName())) {
  auto commitment_scheme_verifier = MakeCommitmentSchemeVerifier<HashT>(
      n_columns * FieldElementT::SizeInBytes(), n_rows, channel,
      n_verifier_friendly_commitment_layers, commitment_hashes);

  return std::make_unique<TableVerifierImpl>(
      field, n_columns, UseMovedValue(std::move(commitment_scheme_verifier)), channel);
}

}  // namespace starkware

#endif  // STARKWARE_STARK_TEST_UTILS_H_
