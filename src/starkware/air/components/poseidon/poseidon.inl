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

#include "starkware/algebra/field_operations.h"

namespace starkware {

template <typename FieldElementT>
std::vector<FieldElementT> PoseidonComponent<FieldElementT>::WriteTrace(
    gsl::span<const FieldElementT> input, uint64_t component_index,
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  ASSERT_RELEASE(input.size() == m_, "Invalid input size.");

  // Init state.
  std::vector<FieldElementT> state{input.begin(), input.end()};
  std::vector<FieldElementT> state_squared = FieldElementT::UninitializedVector(m_);
  std::vector<FieldElementT> state_cubed = FieldElementT::UninitializedVector(m_);

  // First full rounds.
  for (size_t round = 0; round < SafeDiv(rounds_full_, 2); ++round) {
    // Add round key and cube state.
    for (size_t i = 0; i < m_; ++i) {
      state[i] += ark_[round][i];
      state_squared[i] = state[i] * state[i];
      state_cubed[i] = state[i] * state_squared[i];
    }

    // Store state and its squares in virtual columns.
    for (size_t i = 0; i < m_; ++i) {
      full_rounds_state_[i].SetCell(
          trace, round + rounds_full_capacity_ * component_index, state[i]);
      full_rounds_state_squared_[i].SetCell(
          trace, round + rounds_full_capacity_ * component_index, state_squared[i]);
    }

    // Apply linear transformation.
    ::starkware::LinearTransformation<FieldElementT>(mds_, state_cubed, state);
  }

  // Partial rounds.
  std::vector<FieldElementT> tmp_state = FieldElementT::UninitializedVector(m_);
  size_t round_key_idx = SafeDiv(rounds_full_, 2);
  for (size_t part = 0; part < r_p_partition_.size(); ++part) {
    size_t first_round = part == 0 ? 0 : m_;
    for (size_t round = first_round; round < r_p_partition_[part]; ++round, ++round_key_idx) {
      // Add round key and square last element.
      for (size_t i = 0; i < m_; ++i) {
        tmp_state[i] = state[i] + ark_[round_key_idx][i];
      }
      const FieldElementT last_element_squared = tmp_state.back() * tmp_state.back();

      // Store state in virtual columns.
      partial_rounds_state_[part].SetCell(
          trace, round + r_p_capacities_[part] * component_index, tmp_state.back());
      partial_rounds_state_squared_[part].SetCell(
          trace, round + r_p_capacities_[part] * component_index, last_element_squared);

      // In the last m_ rounds of each part, we write the first m_ rounds of the next part.
      if (part < r_p_partition_.size() - 1 && round >= r_p_partition_[part] - m_) {
        const size_t index = round - r_p_partition_[part] + m_;
        partial_rounds_state_[part + 1].SetCell(
            trace, index + r_p_capacities_[part + 1] * component_index, tmp_state.back());
        partial_rounds_state_squared_[part + 1].SetCell(
            trace, index + r_p_capacities_[part + 1] * component_index, last_element_squared);
      }

      // Cube state.
      tmp_state.back() *= last_element_squared;

      // Apply linear transformation.
      ::starkware::LinearTransformation<FieldElementT>(mds_, tmp_state, state);
    }
  }

  // Last full rounds.
  for (size_t round = 0; round < SafeDiv(rounds_full_, 2); ++round, ++round_key_idx) {
    // Add round key and square state.
    for (size_t i = 0; i < m_; ++i) {
      state[i] += ark_[round_key_idx][i];
      state_squared[i] = state[i] * state[i];
      state_cubed[i] = state[i] * state_squared[i];
    }

    // Store state in virtual columns.
    for (size_t i = 0; i < m_; ++i) {
      full_rounds_state_[i].SetCell(
          trace, round + rounds_full_half_capacity_ + rounds_full_capacity_ * component_index,
          state[i]);
      full_rounds_state_squared_[i].SetCell(
          trace, round + rounds_full_half_capacity_ + rounds_full_capacity_ * component_index,
          state_squared[i]);
    }

    // Apply linear transformation.
    ::starkware::LinearTransformation<FieldElementT>(mds_, state_cubed, state);
  }

  return state;
}

}  // namespace starkware
