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

#include "starkware/fri/fri_prover.h"

#include <set>

#include "starkware/algebra/lde/lde.h"
#include "starkware/algebra/polymorphic/field_element_vector.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/profiling.h"

namespace starkware {

using starkware::fri::details::ChooseQueryIndices;

FriProver::FriProver(
    MaybeOwnedPtr<ProverChannel> channel, MaybeOwnedPtr<TableProverFactory> table_prover_factory,
    MaybeOwnedPtr<FriParameters> params, FieldElementVector&& witness,
    MaybeOwnedPtr<FirstLayerCallback> first_layer_queries_callback,
    MaybeOwnedPtr<const FriProverConfig> fri_prover_config)
    : channel_(std::move(channel)),
      table_prover_factory_(std::move(table_prover_factory)),
      params_(std::move(params)),
      folder_(fri::details::FriFolderFromField(params_->field)),
      witness_(std::move(witness)),
      fri_prover_config_(std::move(fri_prover_config)),
      n_layers_(params_->fri_step_list.size()) {
  committed_layers_.reserve(n_layers_);
  committed_layers_.emplace_back(UseMovedValue(FriCommittedLayerByCallback(
      params_->fri_step_list.at(0), std::move(first_layer_queries_callback))));
}

MaybeOwnedPtr<const FriLayer> FriProver::CommitmentPhase() {
  ProfilingBlock profiling_block("FRI commit phase");
  size_t basis_index = 0;
  bool first_in_memory = true;

  size_t coset_size = witness_.Size();
  uint64_t in_memory_fri_elements =
      Pow2(fri_prover_config_->log_n_max_in_memory_fri_layer_elements);

  // Add first layer (layer 0):
  MaybeOwnedPtr<const FriLayer> first_layer(
      UseMovedValue(FriLayerOutOfMemory(std::move(witness_), UseOwned(params_->fft_bases))));
  MaybeOwnedPtr<const FriLayer> current_layer = UseOwned(first_layer);

  for (size_t layer_num = 1; layer_num <= n_layers_; ++layer_num) {
    const size_t fri_step = params_->fri_step_list[layer_num - 1];
    const size_t next_fri_step = (layer_num < n_layers_) ? params_->fri_step_list[layer_num] : 0;
    bool is_chunk_too_small = current_layer->ChunkSize() / Pow2(next_fri_step + fri_step) < 2;
    bool is_in_memory = current_layer->LayerSize() <= in_memory_fri_elements ||
                        layer_num == n_layers_ || is_chunk_too_small;

    ASSERT_RELEASE(layer_num == 1 || fri_step != 0, "layer_num is not zero but fri step is");
    ASSERT_RELEASE(layer_num < n_layers_ || is_in_memory, "last layer must be in memory");

    AnnotationScope scope(channel_.get(), "Layer " + std::to_string(layer_num));

    current_layer = CreateNextFriLayer(std::move(current_layer), fri_step, &basis_index);

    if (is_in_memory) {
      if (first_in_memory && fri_step != 0) {
        // Optimize creation of 1st in memory layer by creating out of memory layer just before it.
        // This makes the first LDE smaller.
        coset_size = SafeDiv(coset_size, Pow2(fri_step));
        current_layer = UseMovedValue(FriLayerOutOfMemory(std::move(current_layer), coset_size));
      }
      first_in_memory = false;
      current_layer = UseMovedValue(FriLayerInMemory(std::move(current_layer)));
    } else {
      coset_size = SafeDiv(coset_size, Pow2(fri_step));
      current_layer = UseMovedValue(FriLayerOutOfMemory(std::move(current_layer), coset_size));
    }

    MaybeOwnedPtr<const FriLayer> new_layer = UseOwned(current_layer);

    // For last layer, skip creating committed layer:
    if (layer_num == n_layers_) {
      break;
    }

    // Create committed layer:
    committed_layers_.emplace_back(UseMovedValue(FriCommittedLayerByTableProver(
        next_fri_step, std::move(current_layer), *table_prover_factory_, *params_, layer_num)));
    current_layer = UseOwned(new_layer);
  }

  return current_layer;
}

// Generates the next FriLayer.
// Done by creating middleware proxy layers and returning the last of them.
MaybeOwnedPtr<const FriLayer> FriProver::CreateNextFriLayer(
    MaybeOwnedPtr<const FriLayer> current_layer, size_t fri_step, size_t* basis_index) {
  std::optional<FieldElement> eval_point = std::nullopt;
  if (fri_step != 0) {
    eval_point = channel_->ReceiveFieldElement(params_->field, "Evaluation point");
  }

  for (size_t j = 0; j < fri_step; j++, (*basis_index)++) {
    current_layer = UseMovedValue(
        FriLayerProxy(*folder_, std::move(current_layer), *eval_point, fri_prover_config_.get()));
    eval_point = params_->fft_bases->ApplyBasisTransform(*eval_point, *basis_index);
  }

  return current_layer;
}

void FriProver::SendLastLayer(MaybeOwnedPtr<const FriLayer>&& last_layer) {
  AnnotationScope scope(channel_.get(), "Last Layer");
  // If the original witness was of the correct degree, the last layer should be of degree <
  // last_layer_degree_bound.
  const uint64_t degree_bound = params_->last_layer_degree_bound;

  size_t last_layer_basis_index = Sum(params_->fri_step_list);
  std::unique_ptr<FftBases> lde_bases =
      params_->fft_bases->FromLayerAsUniquePtr(last_layer_basis_index);

  std::unique_ptr<LdeManager> lde_manager = MakeLdeManager(*lde_bases);
  FieldElementVector last_layer_evaluations = last_layer->GetAllEvaluation();
  VLOG(3) << "FRI Size of last layer: " << last_layer_evaluations.Size();
  VLOG(3) << "FRI Last layer: " << last_layer_evaluations;
  lde_manager->AddEvaluation(std::move(last_layer_evaluations));

  int64_t degree = lde_manager->GetEvaluationDegree(0);
  ASSERT_RELEASE(
      degree < (int64_t)degree_bound, "Last FRI layer is of degree: " + std::to_string(degree) +
                                          ". Expected degree < " + std::to_string(degree_bound));

  const auto coefs = lde_manager->GetCoefficients(0);
  channel_->SendFieldElementSpan(coefs.SubSpan(0, degree_bound), "Coefficients");
}

void FriProver::ProveFri() {
  // Commitment phase.
  {
    AnnotationScope scope(channel_.get(), "Commitment");
    MaybeOwnedPtr<const FriLayer> last_layer = CommitmentPhase();
    SendLastLayer(std::move(last_layer));
  }

  // Query phase.
  std::vector<uint64_t> queries = ChooseQueryIndices(
      channel_.get(), params_->fft_bases->At(params_->fri_step_list.at(0)).Size(),
      params_->n_queries, params_->proof_of_work_bits);
  // It is not allowed for the verifier to send randomness to the prover after the following line.

  channel_->BeginQueryPhase();

  // Decommitment phase.
  AnnotationScope scope(channel_.get(), "Decommitment");

  ProfilingBlock profiling_block("FRI response generation");
  size_t layer_num = 0;
  for (auto& layer : committed_layers_) {
    AnnotationScope scope(channel_.get(), "Layer " + std::to_string(layer_num++));
    layer->Decommit(queries);
  }
}

}  // namespace starkware
