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

#include "starkware/fri/fri_details.h"

#include <algorithm>
#include <utility>

#include "starkware/algebra/lde/lde.h"
#include "starkware/algebra/utils/invoke_template_version.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/math/math.h"
#include "starkware/utils/profiling.h"

namespace starkware {
namespace fri {
namespace details {

FieldElement ApplyFriLayers(
    const ConstFieldElementSpan& elements, const std::optional<FieldElement>& eval_point,
    const FriParameters& params, size_t layer_num, uint64_t first_element_index,
    const FriFolderBase& folder) {
  std::optional<FieldElement> curr_eval_point = eval_point;
  // Find the first relevant basis for the requested layer.
  size_t cumulative_fri_step = 0;
  for (size_t i = 0; i < layer_num; ++i) {
    cumulative_fri_step += params.fri_step_list[i];
  }

  const size_t layer_fri_step = params.fri_step_list[layer_num];
  ASSERT_RELEASE(
      elements.Size() == Pow2(layer_fri_step),
      "Number of elements is not consistent with the fri_step parameter.");
  FieldElementVector cur_layer = FieldElementVector::CopyFrom(elements);
  for (size_t basis_index = cumulative_fri_step; basis_index < cumulative_fri_step + layer_fri_step;
       basis_index++) {
    ASSERT_RELEASE(curr_eval_point.has_value(), "evaluation point doesn't have a value");
    // Apply NextLayerElementFromTwoPreviousLayerElements() on pairs of field elements to compute
    // the next inner layer.
    const FftDomainBase& basis = params.fft_bases->At(basis_index);

    FieldElementVector next_layer = FieldElementVector::Make(elements.GetField());
    next_layer.Reserve(cur_layer.Size() / 2);
    for (size_t j = 0; j < cur_layer.Size(); j += 2) {
      next_layer.PushBack(folder.NextLayerElementFromTwoPreviousLayerElements(
          cur_layer[j], cur_layer[j + 1], *curr_eval_point,
          basis.GetFieldElementAt(first_element_index + j)));
    }

    // Update the variables for the next iteration.
    cur_layer = std::move(next_layer);
    *curr_eval_point = params.fft_bases->ApplyBasisTransform(*curr_eval_point, basis_index);
    first_element_index /= 2;
  }

  ASSERT_RELEASE(cur_layer.Size() == 1, "Expected number of elements to be one.");
  return cur_layer[0];
}

std::vector<uint64_t> ChooseQueryIndices(
    Channel* channel, uint64_t domain_size, size_t n_queries, size_t proof_of_work_bits) {
  // Proof of work right before randomizing queries.
  channel->ApplyProofOfWork(proof_of_work_bits);

  AnnotationScope scope(channel, "QueryIndices");

  std::vector<uint64_t> query_indices;
  query_indices.reserve(n_queries);
  for (size_t i = 0; i < n_queries; ++i) {
    query_indices.push_back(channel->GetRandomNumberFromVerifier(domain_size, std::to_string(i)));
  }
  std::sort(query_indices.begin(), query_indices.end());
  return query_indices;
}

void NextLayerDataAndIntegrityQueries(
    const std::vector<uint64_t>& query_indices, const FriParameters& params, size_t layer_num,
    std::set<RowCol>* data_queries, std::set<RowCol>* integrity_queries) {
  // cumulative_fri_step is the sum of fri_step starting from the second layer and up to the
  // requested layer. It allows us to compute the indices of the queries in the requested layer,
  // given the indices of the second layer.
  size_t cumulative_fri_step = 0;
  for (size_t i = 1; i < layer_num; ++i) {
    cumulative_fri_step += params.fri_step_list[i];
  }
  const size_t layer_fri_step = params.fri_step_list[layer_num];

  for (uint64_t idx : query_indices) {
    integrity_queries->insert(GetTableProverRowCol(idx >> cumulative_fri_step, layer_fri_step));
  }
  for (uint64_t idx : query_indices) {
    // Find the first element of the coset: Divide idx by 2^cumulative_fri_step to find the query
    // location in the current layer, then clean the lower bits to get the first query in the coset.
    const uint64_t coset_row = GetTableProverRow(idx >> cumulative_fri_step, layer_fri_step);
    for (size_t coset_col = 0; coset_col < Pow2(layer_fri_step); coset_col++) {
      RowCol query{coset_row, coset_col};
      // Add the query to data_queries only if it is not an integrity query.
      if (integrity_queries->count(query) == 0) {
        data_queries->insert(query);
      }
    }
  }
}

std::vector<uint64_t> SecondLayerQeuriesToFirstLayerQueries(
    const std::vector<uint64_t>& query_indices, size_t first_fri_step) {
  std::vector<uint64_t> first_layer_queries;
  const size_t first_layer_coset_size = Pow2(first_fri_step);
  first_layer_queries.reserve(query_indices.size() * first_layer_coset_size);
  for (uint64_t idx : query_indices) {
    for (uint64_t i = idx * first_layer_coset_size; i < (idx + 1) * first_layer_coset_size; ++i) {
      first_layer_queries.push_back(i);
    }
  }
  return first_layer_queries;
}

}  // namespace details
}  // namespace fri
}  // namespace starkware
