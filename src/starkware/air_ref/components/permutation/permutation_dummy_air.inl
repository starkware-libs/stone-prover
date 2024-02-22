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

#include "starkware/air/components/permutation/permutation.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/field_operations.h"

namespace starkware {

template <typename FieldElementT, int LayoutId>
std::unique_ptr<const PermutationDummyAir<FieldElementT, LayoutId>>
PermutationDummyAir<FieldElementT, LayoutId>::WithInteractionElementsImpl(
    const gsl::span<const FieldElementT>& interaction_elms) const {
  auto new_air = std::make_unique<PermutationDummyAir>(*this);
  ASSERT_RELEASE(interaction_elms.size() == 3, "Interaction element vector is of wrong size.");
  new_air->multi_column_perm__perm__interaction_elm_ = interaction_elms[0];
  new_air->multi_column_perm__hash_interaction_elm0_ = interaction_elms[1];
  new_air->multi_column_perm__hash_interaction_elm1_ = interaction_elms[2];
  return new_air;
}

template <typename FieldElementT, int LayoutId>
Trace PermutationDummyAir<FieldElementT, LayoutId>::GetTrace() const {
  // Allocate memory to first trace.
  std::vector<std::vector<FieldElementT>> trace_values(
      this->kNumColumnsFirst,
      std::vector<FieldElementT>(this->trace_length_, FieldElementT::Zero()));
  // Compute a random permutation of the numbers between 0 and trace_length-1.
  std::vector<size_t> perm_indices(this->trace_length_);
  for (size_t j = 0; j < this->trace_length_; j++) {
    perm_indices[j] = j;
  }
  // Permutate indices.
  std::shuffle(perm_indices.begin(), perm_indices.end(), std::default_random_engine(0));
  // Allocate random numbers to original columns - first kNOriginalCols cols and a permutation on
  // them using perm_indices.
  ASSERT_RELEASE(
      this->kNumColumnsFirst == 2 * this->kNOriginalCols,
      "Number of columns of the first trace is not equal to twice the number of original columns.");
  for (size_t j = 0; j < this->trace_length_; j++) {
    for (size_t k = 0; k < this->kNOriginalCols; k++) {
      auto value = FieldElementT::RandomElement(prng_);
      trace_values[k][j] = value;
      trace_values[k + this->kNOriginalCols][perm_indices[j]] = value;
    }
  }
  return Trace(std::move(trace_values));
}

template <typename FieldElementT, int LayoutId>
Trace PermutationDummyAir<FieldElementT, LayoutId>::GetInteractionTrace(
    gsl::span<const gsl::span<const FieldElementT>> originals,
    gsl::span<const gsl::span<const FieldElementT>> perms) const {
  // Validate the size of the input.
  ASSERT_RELEASE(
      originals.size() == this->kNOriginalCols && perms.size() == this->kNOriginalCols,
      "Number of original columns and number of perm columns are different.");
  for (size_t i = 0; i < this->kNOriginalCols; ++i) {
    ASSERT_RELEASE(
        originals[i].size() == this->trace_length_ && perms[i].size() == this->trace_length_,
        "Length of old col is wrong");
  }

  std::vector<std::vector<FieldElementT>> trace = {
      std::vector<FieldElementT>(this->trace_length_, FieldElementT::Zero())};

  const std::vector<FieldElementT> interaction_elms_vec = {
      this->multi_column_perm__perm__interaction_elm_,
      this->multi_column_perm__hash_interaction_elm0_,
      this->multi_column_perm__hash_interaction_elm1_};

  ASSERT_RELEASE(this->kNSeries == 1, "Only one series is supported");

  MultiColumnPermutationComponent<FieldElementT> multi_permutation(
      "multi_column_perm", 1, this->GetTraceGenerationContext());
  multi_permutation.WriteInteractionTrace(
      {originals}, {perms}, interaction_elms_vec, SpanAdapter(trace), FieldElementT::One());
  return Trace(std::move(trace));
}

}  // namespace starkware
