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

#ifndef STARKWARE_FRI_FRI_DETAILS_H_
#define STARKWARE_FRI_FRI_DETAILS_H_

#include <memory>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/algebra/polymorphic/field_element_vector.h"
#include "starkware/channel/channel.h"
#include "starkware/commitment_scheme/row_col.h"
#include "starkware/fft_utils/fft_bases.h"
#include "starkware/fri/fri_folder.h"
#include "starkware/fri/fri_parameters.h"
#include "starkware/utils/json.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {
namespace fri {
namespace details {

/*
  Computes the element from the next FRI layer, given the corresponding coset from the current
  layer.
  For example, if fri_step_list[layer_num] = 1, this function behaves the same as
  NextLayerElementFromTwoPreviousLayerElements().
*/
FieldElement ApplyFriLayers(
    const ConstFieldElementSpan& elements, const std::optional<FieldElement>& eval_point,
    const FriParameters& params, size_t layer_num, uint64_t first_element_index,
    const FriFolderBase& folder);

/*
  Given query indices that refer to FRI's second layer, compute the indices of the cosets in the
  first layer.
*/
std::vector<uint64_t> SecondLayerQeuriesToFirstLayerQueries(
    const std::vector<uint64_t>& query_indices, size_t first_fri_step);

/*
  Given the query indices (of FRI's second layer), we compute the data queries and integrity queries
  for the next layer of FRI.
  Data queries are queries whose data needs to go over the channel.
  Integrity queries are ones that each party can compute based on previously known information.

  For example, if fri_step of the corresponding layer is 3, then the size of the coset is 8. The
  verifier will be able to compute one element (integrity query) and the other 7 will be sent in the
  channel (data queries).

  Note: The two resulting sets are disjoint.
*/
void NextLayerDataAndIntegrityQueries(
    const std::vector<uint64_t>& query_indices, const FriParameters& params, size_t layer_num,
    std::set<RowCol>* data_queries, std::set<RowCol>* integrity_queries);

std::vector<uint64_t> ChooseQueryIndices(
    Channel* channel, uint64_t domain_size, size_t n_queries, size_t proof_of_work_bits);

// Given the query index in the layer (1D), calculate the cell position in the 2D table
// according to coset size (always power of 2).
// fri_step is a log2 of coset size (row_size).

/*
  Logic: query_index >> fri_step == query_index / Pow2(fri_step) == query_index / row_size.
*/
inline uint64_t GetTableProverRow(const uint64_t query_index, const size_t fri_step) {
  return query_index >> fri_step;
}

/*
  Logic: query_index & (Pow2(fri_step) - 1) == query_index % row_size
  (Pow2(fri_step) - 1) is a mask of 1s to the row_size.
*/
inline uint64_t GetTableProverCol(const uint64_t query_index, const size_t fri_step) {
  return query_index & (Pow2(fri_step) - 1);
}

inline RowCol GetTableProverRowCol(const uint64_t query_index, const size_t fri_step) {
  return {GetTableProverRow(query_index, fri_step), GetTableProverCol(query_index, fri_step)};
}

}  // namespace details
}  // namespace fri
}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_DETAILS_H_
