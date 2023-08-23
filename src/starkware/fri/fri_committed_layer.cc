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

#include "starkware/fri/fri_committed_layer.h"

#include <set>

#include "starkware/fri/fri_details.h"

namespace starkware {

using starkware::fri::details::NextLayerDataAndIntegrityQueries;
using starkware::fri::details::SecondLayerQeuriesToFirstLayerQueries;

//  ---------------------------------------------------------
//  Class FriCommittedLayerByCallback.

void FriCommittedLayerByCallback::Decommit(const std::vector<uint64_t>& queries) {
  (*layer_queries_callback_)(SecondLayerQeuriesToFirstLayerQueries(queries, fri_step_));
}

//  ---------------------------------------------------------
//  Class FriCommittedLayerByTableProver.

FriCommittedLayerByTableProver::FriCommittedLayerByTableProver(
    size_t fri_step, MaybeOwnedPtr<const FriLayer> layer,
    const TableProverFactory& table_prover_factory, const FriParameters& params, size_t layer_num)
    : FriCommittedLayer(fri_step),
      fri_layer_(std::move(layer)),
      params_(params),
      layer_num_(layer_num) {
  uint64_t chunk_size = fri_layer_->ChunkSize();
  uint64_t n_chunks = SafeDiv(fri_layer_->LayerSize(), chunk_size);
  table_prover_ = table_prover_factory(n_chunks, chunk_size / Pow2(fri_step_), Pow2(fri_step_));
  ASSERT_RELEASE(table_prover_, "No table prover for current layer (probably last layer)");
  Commit();
}

FriCommittedLayerByTableProver::ElementsData FriCommittedLayerByTableProver::EvalAtPoints(
    const gsl::span<uint64_t>& required_row_indices) {
  // Create a vector and a span totally matching. Span cannot be independent.
  // It's needed in order to have the ability of getting subspans.
  ElementsData elements_data;
  auto& [elements_data_spans, elements_data_vectors] = elements_data;

  size_t coset_size = Pow2(params_.fri_step_list[layer_num_]);
  elements_data_vectors.reserve(coset_size);
  elements_data_spans.reserve(coset_size);

  for (uint64_t col = 0; col < coset_size; col++) {
    std::vector<uint64_t> required_indices;
    for (uint64_t row : required_row_indices) {
      required_indices.push_back(row * coset_size + col);
    }

    FieldElementVector res = fri_layer_->EvalAtPoints(required_indices);
    elements_data_vectors.push_back(std::move(res));
    elements_data_spans.emplace_back(elements_data_vectors[col]);
  }
  return elements_data;
}

void FriCommittedLayerByTableProver::Commit() {
  uint64_t chunk_size = fri_layer_->ChunkSize();
  uint64_t n_chunks = SafeDiv(fri_layer_->LayerSize(), chunk_size);
  auto storage = fri_layer_->MakeStorage();
  for (uint64_t index = 0; index < n_chunks; ++index) {
    ConstFieldElementSpan chunk = fri_layer_->GetChunk(storage.get(), chunk_size, index);
    table_prover_->AddSegmentForCommitment({chunk}, index, Pow2(fri_step_));
  }
  table_prover_->Commit();
}

void FriCommittedLayerByTableProver::Decommit(const std::vector<uint64_t>& queries) {
  std::set<RowCol> layer_data_queries, layer_integrity_queries;
  NextLayerDataAndIntegrityQueries(
      queries, params_, layer_num_, &layer_data_queries, &layer_integrity_queries);
  std::vector<uint64_t> required_row_indices =
      table_prover_->StartDecommitmentPhase(layer_data_queries, layer_integrity_queries);

  ElementsData elements_data(EvalAtPoints(required_row_indices));

  table_prover_->Decommit(elements_data.elements);
}

}  // namespace starkware
