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

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

template <typename FieldElementT>
PermutationComponent<FieldElementT>::PermutationComponent(
    const std::string& name, const size_t n_series, const TraceGenerationContext& ctx)
    : n_series_(n_series) {
  cum_prod_cols_.reserve(n_series);
  for (size_t i = 0; i < n_series; ++i) {
    cum_prod_cols_.push_back(ctx.GetVirtualColumn(name + "/cum_prod" + std::to_string(i)));
  }
}

template <typename FieldElementT>
void PermutationComponent<FieldElementT>::WriteInteractionTrace(
    gsl::span<const gsl::span<const FieldElementT>> origs,
    gsl::span<const gsl::span<const FieldElementT>> perms, FieldElementT interaction_elm,
    gsl::span<const gsl::span<FieldElementT>> interaction_trace,
    const FieldElementT& expected_public_memory_prod) const {
  ASSERT_RELEASE(
      !interaction_trace.empty(), "Interaction trace given to WriteInteractionTrace is empty.");
  ASSERT_RELEASE(origs.size() == n_series_, "Wrong number of original series.");
  ASSERT_RELEASE(perms.size() == n_series_, "Wrong number of permutation series.");

  size_t total_size = 0;
  for (size_t series_i = 0; series_i < n_series_; ++series_i) {
    ASSERT_RELEASE(
        origs[series_i].size() == perms[series_i].size(),
        "Non matching size of original and permutation series");
    ASSERT_RELEASE(
        origs[series_i].size() == cum_prod_cols_[series_i].Size(interaction_trace[0].size()),
        "series size not matching size of cum_prod_cols_");
    total_size += origs[series_i].size();
  }

  std::vector<FieldElementT> shifted_perm;
  shifted_perm.reserve(total_size);

  for (size_t series_i = 0; series_i < n_series_; ++series_i) {
    for (size_t i = 0; i < perms[series_i].size(); ++i) {
      shifted_perm.push_back(interaction_elm - perms[series_i][i]);
    }
  }
  // Batch inverse.
  std::vector<FieldElementT> shifted_perm_inverse =
      FieldElementT::UninitializedVector(shifted_perm.size());
  BatchInverse<FieldElementT>(shifted_perm, shifted_perm_inverse);

  // Value in cell i+1 of interaction column cumprod is
  // cumprod[i]*(interaction_elm - original[i]) / (interaction_elm - perm[i]).
  auto val = FieldElementT::One();
  size_t perm_offset = 0;
  for (size_t series_i = 0; series_i < n_series_; ++series_i) {
    for (size_t i = 0; i < perms[series_i].size(); ++i, ++perm_offset) {
      val *= shifted_perm_inverse[perm_offset] * (interaction_elm - origs[series_i][i]);
      cum_prod_cols_[series_i].SetCell(interaction_trace, i, val);
    }
  }

  // Check that the last value in the cum_prod column is indeed as expected.
  ASSERT_RELEASE(
      val == expected_public_memory_prod, "Last value in cum_prod column is wrong. Expected: " +
                                              expected_public_memory_prod.ToString() +
                                              " , actual: " + val.ToString() + ".");
}

}  // namespace starkware
