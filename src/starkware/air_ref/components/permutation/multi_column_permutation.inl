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
void MultiColumnPermutationComponent<FieldElementT>::WriteInteractionTrace(
    gsl::span<const gsl::span<const gsl::span<const FieldElementT>>> originals,
    gsl::span<const gsl::span<const gsl::span<const FieldElementT>>> perms,
    gsl::span<const FieldElementT> interaction_elms,
    gsl::span<const gsl::span<FieldElementT>> interaction_trace,
    const FieldElementT& expected_public_memory_prod) const {
  ASSERT_RELEASE(
      interaction_elms.size() == originals[0].size(), "Wrong number of interaction elements.");
  ASSERT_RELEASE(
      !interaction_trace.empty(), "Interaction trace given to WriteInteractionTrace is empty.");
  ASSERT_RELEASE(
      originals.size() == perms.size(), "Number of original series and perms series is different.");

  std::vector<std::vector<FieldElementT>> combined_originals;
  std::vector<std::vector<FieldElementT>> combined_perms;
  combined_originals.reserve(originals.size());
  combined_perms.reserve(originals.size());

  for (size_t series_i = 0; series_i < originals.size(); ++series_i) {
    ASSERT_RELEASE(
        originals[series_i].size() == perms[series_i].size(),
        "Number of original columns and perms columns is different.");

    const size_t input_cols_length = originals[series_i][0].size();

    for (size_t i = 0; i < originals.size(); ++i) {
      ASSERT_RELEASE(
          originals[series_i][i].size() == input_cols_length, "original column has a wrong size.");
      ASSERT_RELEASE(
          perms[series_i][i].size() == input_cols_length, "perm column has a wrong size.");
    }

    std::vector<FieldElementT> combined_original;
    std::vector<FieldElementT> combined_perm;
    combined_original.reserve(input_cols_length);
    combined_perm.reserve(input_cols_length);

    // Create two linear combinations from original columns and perm columns - combined_original and
    // combined_perm. These combinations will be sent to the permutation component.
    for (size_t i = 0; i < input_cols_length; ++i) {
      auto val_orig = originals[series_i][0][i];
      auto val_perm = perms[series_i][0][i];
      for (size_t elm_idx = 1; elm_idx < interaction_elms.size(); elm_idx++) {
        val_orig += interaction_elms[elm_idx] * originals[series_i][elm_idx][i];
        val_perm += interaction_elms[elm_idx] * perms[series_i][elm_idx][i];
      }
      combined_original.push_back(val_orig);
      combined_perm.push_back(val_perm);
    }

    combined_originals.push_back(std::move(combined_original));
    combined_perms.push_back(std::move(combined_perm));
  }

  perm_component_.WriteInteractionTrace(
      ConstSpanAdapter(combined_originals), ConstSpanAdapter(combined_perms), interaction_elms[0],
      interaction_trace, expected_public_memory_prod);
}

}  // namespace starkware
