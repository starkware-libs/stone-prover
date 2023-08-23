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

#include "starkware/fri/fri_verifier.h"

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "starkware/algebra/lde/lde.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/commitment_scheme/table_impl_details.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

using starkware::fri::details::ApplyFriLayers;
using starkware::fri::details::ChooseQueryIndices;
using starkware::fri::details::GetTableProverRow;
using starkware::fri::details::GetTableProverRowCol;
using starkware::fri::details::NextLayerDataAndIntegrityQueries;
using starkware::fri::details::SecondLayerQeuriesToFirstLayerQueries;
using starkware::table::details::ElementDecommitAnnotation;

void FriVerifier::Init() {
  eval_points_.reserve(n_layers_ - 1);
  table_verifiers_.reserve(n_layers_ - 1);
  query_results_.reserve(params_->n_queries);
}

void FriVerifier::CommitmentPhase() {
  size_t basis_index = 0;
  for (size_t i = 0; i < n_layers_; i++) {
    size_t cur_fri_step = params_->fri_step_list[i];
    AnnotationScope scope(channel_.get(), "Layer " + std::to_string(i + 1));
    basis_index += cur_fri_step;
    if (i == 0) {
      if (params_->fri_step_list[0] != 0) {
        first_eval_point_ =
            channel_->GetAndSendRandomFieldElement(params_->field, "Evaluation point");
      }
    } else {
      eval_points_.push_back(
          channel_->GetAndSendRandomFieldElement(params_->field, "Evaluation point"));
    }
    if (i < n_layers_ - 1) {
      size_t coset_size = Pow2(params_->fri_step_list[i + 1]);
      std::unique_ptr<TableVerifier> commitment_scheme = (*table_verifier_factory_)(
          params_->field, params_->fft_bases->At(basis_index).Size() / coset_size, coset_size);
      commitment_scheme->ReadCommitment();
      table_verifiers_.push_back(std::move(commitment_scheme));
    }
  }
}

void FriVerifier::ReadLastLayerCoefficients() {
  AnnotationScope scope(channel_.get(), "Last Layer");
  const size_t fri_step_sum = Sum(params_->fri_step_list);
  const uint64_t last_layer_size = params_->fft_bases->At(fri_step_sum).Size();

  // Allocate a vector of zeros of size last_layer_size and fill the first last_layer_degree_bound
  // elements.
  FieldElementVector last_layer_coefficients_vector =
      FieldElementVector::Make(last_layer_size, params_->field.Zero());
  channel_->ReceiveFieldElementSpan(
      params_->field,
      last_layer_coefficients_vector.AsSpan().SubSpan(0, params_->last_layer_degree_bound),
      "Coefficients");

  ASSERT_RELEASE(
      params_->last_layer_degree_bound <= last_layer_size,
      "last_layer_degree_bound (" + std::to_string(params_->last_layer_degree_bound) +
          ") must be <= last_layer_size (" + std::to_string(last_layer_size) + ").");

  size_t last_layer_basis_index = Sum(params_->fri_step_list);
  std::unique_ptr<FftBases> lde_bases =
      params_->fft_bases->FromLayerAsUniquePtr(last_layer_basis_index);

  std::unique_ptr<LdeManager> last_layer_lde = MakeLdeManager(*lde_bases);

  last_layer_lde->AddFromCoefficients(last_layer_coefficients_vector);
  expected_last_layer_ = FieldElementVector::MakeUninitialized(params_->field, last_layer_size);

  // Coset offset can be taken as first element in lde_bases.
  last_layer_lde->EvalOnCoset(
      lde_bases->At(0).GetFieldElementAt(0), std::vector<FieldElementSpan>{*expected_last_layer_});
}

void FriVerifier::VerifyFirstLayer() {
  AnnotationScope scope(channel_.get(), "Layer 0");
  const size_t first_fri_step = params_->fri_step_list.at(0);
  std::vector<uint64_t> first_layer_queries =
      SecondLayerQeuriesToFirstLayerQueries(query_indices_, first_fri_step);
  const FieldElementVector first_layer_results =
      (*first_layer_queries_callback_)(first_layer_queries);
  ASSERT_RELEASE(
      first_layer_results.Size() == first_layer_queries.size(),
      "Returned number of queries does not match the number sent");
  const size_t first_layer_coset_size = Pow2(first_fri_step);
  for (size_t i = 0; i < first_layer_queries.size(); i += first_layer_coset_size) {
    query_results_.push_back(ApplyFriLayers(
        first_layer_results.AsSpan().SubSpan(i, first_layer_coset_size), first_eval_point_,
        *params_, 0, first_layer_queries[i], *folder_));
  }
}

void FriVerifier::VerifyInnerLayers() {
  const size_t first_fri_step = params_->fri_step_list.at(0);
  size_t basis_index = 0;
  for (size_t i = 0; i < n_layers_ - 1; ++i) {
    AnnotationScope scope(channel_.get(), "Layer " + std::to_string(i + 1));

    const size_t cur_fri_step = params_->fri_step_list[i + 1];
    basis_index += params_->fri_step_list[i];

    std::set<RowCol> layer_data_queries, layer_integrity_queries;
    NextLayerDataAndIntegrityQueries(
        query_indices_, *params_, i + 1, &layer_data_queries, &layer_integrity_queries);
    // Collect results for data queries.
    std::map<RowCol, FieldElement> to_verify =
        table_verifiers_[i]->Query(layer_data_queries, layer_integrity_queries);
    const FftDomainBase& basis = params_->fft_bases->At(basis_index);
    uint64_t prev_query_index = std::numeric_limits<uint64_t>::max();
    for (size_t j = 0; j < query_results_.size(); ++j) {
      uint64_t query_index = query_indices_[j] >> (basis_index - first_fri_step);
      const RowCol& query_loc = GetTableProverRowCol(query_index, cur_fri_step);
      to_verify.insert(std::make_pair(query_loc, query_results_[j]));
      // Don't repeat extra annotations for merging query paths.
      if (query_index == prev_query_index || channel_->ExtraAnnotationsDisabled()) {
        continue;
      }
      prev_query_index = query_index;
      // Annotate previous FRI layer for fri-proof-splitting.
      channel_->AnnotateExtraFieldElement(query_results_[j], ElementDecommitAnnotation(query_loc));
      // Compute and annotate the x-inv of query j for fri-proof-splitting.
      FieldElement x_inv = basis.GetFieldElementAt(query_index).Inverse();
      channel_->AnnotateExtraFieldElement(x_inv, "xInv for index " + std::to_string(query_index));
    }
    // Compute next layer.
    const FieldElement& eval_point = eval_points_[i];
    for (size_t j = 0; j < query_results_.size(); ++j) {
      const size_t coset_size = Pow2(cur_fri_step);
      FieldElementVector coset_elements = FieldElementVector::Make(params_->field);
      coset_elements.Reserve(coset_size);
      const uint64_t coset_start =
          GetTableProverRow(query_indices_[j] >> (basis_index - first_fri_step), cur_fri_step);
      for (size_t k = 0; k < coset_size; k++) {
        coset_elements.PushBack(to_verify.at(RowCol(coset_start, k)));
      }
      query_results_[j] = ApplyFriLayers(
          coset_elements, eval_point, *params_, i + 1, coset_start * Pow2(cur_fri_step), *folder_);
    }

    ASSERT_RELEASE(
        table_verifiers_[i]->VerifyDecommitment(to_verify),
        "Layer " + std::to_string(i) + " failed decommitment");
  }
}

void FriVerifier::VerifyLastLayer() {
  const size_t first_fri_step = params_->fri_step_list.at(0);
  const size_t fri_step_sum = Sum(params_->fri_step_list);

  ASSERT_RELEASE(
      expected_last_layer_.has_value(), "ReadLastLayer() must be called before VerifyLastLayer().");

  AnnotationScope scope(channel_.get(), "Last Layer");
  const FftDomainBase& basis = params_->fft_bases->At(fri_step_sum);

  uint64_t prev_query_index = std::numeric_limits<uint64_t>::max();
  for (size_t j = 0; j < query_results_.size(); ++j) {
    const uint64_t query_index = query_indices_[j] >> (fri_step_sum - first_fri_step);
    const FieldElement expected_value = expected_last_layer_->At(query_index);
    ASSERT_RELEASE(
        query_results_[j] == expected_value,
        "FRI query #" + std::to_string(j) +
            " is not consistent with the coefficients of the last layer.");
    // Don't repeat extra annotations for merging query paths.
    if (query_index == prev_query_index || channel_->ExtraAnnotationsDisabled()) {
      continue;
    }
    prev_query_index = query_index;
    // Annotate last FRI layer for fri-proof-splitting.
    channel_->AnnotateExtraFieldElement(
        query_results_[j], "Row " + std::to_string(query_index) + ", Column 0");
    // Compute and annotate the x-inv of query j for fri-proof-splitting.
    FieldElement x_inv = basis.GetFieldElementAt(query_index).Inverse();
    channel_->AnnotateExtraFieldElement(x_inv, "xInv for index " + std::to_string(query_index));
  }
}

void FriVerifier::VerifyFri() {
  Init();
  // Commitment phase.
  {
    AnnotationScope scope(channel_.get(), "Commitment");
    CommitmentPhase();
    ReadLastLayerCoefficients();
  }

  // Query phase.
  query_indices_ = ChooseQueryIndices(
      channel_.get(), params_->fft_bases->At(params_->fri_step_list.at(0)).Size(),
      params_->n_queries, params_->proof_of_work_bits);
  // It is not allowed for the verifier to send randomness to the prover after the following line.
  channel_->BeginQueryPhase();

  // Deommitment phase.
  AnnotationScope scope(channel_.get(), "Decommitment");

  // Since we resolve all queries in parallel, one layer at a time, we store the intermediate
  // results in query_results. That is to say, at the beginning of the process, it stores the
  // responses from the second layer, and by the end of the FRI protocol, it stores the same
  // value in all its elements, since it comes from the last (constant) layer of the FRI.
  VerifyFirstLayer();

  // Inner layers.
  VerifyInnerLayers();

  // Last layer.
  VerifyLastLayer();
}

}  // namespace starkware
