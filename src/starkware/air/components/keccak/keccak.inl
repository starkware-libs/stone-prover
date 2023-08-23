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

#include "starkware/crypt_tools/keccak_256.h"

namespace starkware {

template <typename FieldElementT>
void KeccakComponent<FieldElementT>::AppendBytesToKeccakIO(
    gsl::span<const std::byte> src, std::vector<FieldElementT>* dst) {
  // A buffer to store the zero-padded word in order to convert it to a field element.
  std::array<std::byte, FieldElementT::SizeInBytes()> buffer{};

  for (size_t word = 0; word < kStateSizeInWords; ++word) {
    std::copy(
        src.rbegin() + (kStateSizeInWords - 1 - word) * kBytesInWord,
        src.rbegin() + (kStateSizeInWords - word) * kBytesInWord,
        buffer.begin() + FieldElementT::SizeInBytes() - kBytesInWord);
    dst->push_back(FieldElementT::FromBigInt(FieldElementT::ValueType::FromBytes(buffer)));
  }
}

template <typename FieldElementT>
std::vector<FieldElementT> KeccakComponent<FieldElementT>::WriteTrace(
    gsl::span<const std::byte> input, uint64_t component_index,
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  ASSERT_RELEASE(input.size() == kStateSizeInBytes * n_invocations_, "Invalid input size.");
  // Translate inputs and outputs to field elements.
  std::vector<FieldElementT> input_output;
  // The 2 is because we store both inputs and outputs.
  input_output.reserve(2 * n_invocations_ * kStateSizeInWords);
  for (size_t invocation = 0; invocation < n_invocations_; ++invocation) {
    gsl::span<const std::byte> current_input =
        input.subspan(invocation * kStateSizeInBytes, kStateSizeInBytes);
    AppendBytesToKeccakIO(current_input, &input_output);
    AppendBytesToKeccakIO(Keccak256::ApplyPermutation(current_input), &input_output);
  }

  // Parse inputs and outputs to diluted form.
  parse_to_diluted_.WriteTrace(
      input_output, std::array{state_begin_column_, state_end_column_}, component_index, trace);

  // Init state.
  std::array<std::array<std::array<uint64_t, 64>, 5>, 5> state{};
  for (size_t i = 0; i < 5; ++i) {
    for (size_t j = 0; j < 5; ++j) {
      for (size_t k = 0; k < 64; ++k) {
        state.at(i)[j][k] = state_begin_column_.Get(RowIndex(i, j, k) + 2048 * component_index);
      }
    }
  }

  // Diluted form mask.
  uint64_t diluted_mask = 0;
  for (size_t invocation = 0; invocation < n_invocations_; ++invocation) {
    diluted_mask <<= diluted_spacing_;
    diluted_mask++;
  }

  // Begin computation.
  for (size_t round = 0; round < 24; ++round) {
    // Compute parity bits.
    std::array<std::array<uint64_t, 64>, 5> parities{};
    for (size_t j = 0; j < 5; ++j) {
      for (size_t k = 0; k < 64; ++k) {
        const uint64_t value =
            state[0][j][k] + state[1][j][k] + state[2][j][k] + state[3][j][k] + state[4][j][k];
        parities.at(j)[k] = diluted_mask & value;
        for (size_t b = 0; b < 3; ++b) {
          parity_columns_[b][j].WriteTrace(
              round + 32 * (RowIndex(0, 0, k) + 2048 * component_index),
              diluted_mask & (value >> b), trace);
        }
        rotated_parity_columns_[j].SetCell(
            trace, round + 32 * (RowIndex(0, 0, (k + 1) % 64) + 2048 * component_index),
            FieldElementT::FromUint(diluted_mask & value));
      }
    }

    // Apply Theta + Rho + Pi.
    std::array<std::array<std::array<uint64_t, 64>, 5>, 5> after_theta_rho_pi_state{};
    for (size_t i = 0; i < 5; ++i) {
      for (size_t j = 0; j < 5; ++j) {
        const size_t pi_i = (3 * i + 2 * j) % 5;
        const size_t pi_j = i;
        const size_t n = (round / 8) % theta_aux_columns_[pi_i][pi_j].size();
        const size_t adjusted_round = round - 8 * n;
        for (size_t k = 0; k < 64; ++k) {
          const uint64_t value = state.at(i)[j][k] + parities.at((j + 4) % 5)[k] +
                                 parities.at((j + 1) % 5)[(k + 63) % 64];
          const size_t rho_k = (k + kOffsets.at(i)[j]) % 64;
          after_theta_rho_pi_state.at(pi_i)[pi_j][rho_k] = diluted_mask & value;
          after_theta_rho_pi_column_.WriteTrace(
              round + 32 * (RowIndex(pi_i, pi_j, rho_k) + 2048 * component_index),
              diluted_mask & value, trace);
          theta_aux_columns_[pi_i][pi_j][n].WriteTrace(
              adjusted_round + 32 * (RowIndex(0, 0, rho_k) + 2048 * component_index),
              diluted_mask & (value >> 1), trace);
        }
      }
    }

    // Apply Chi + Iota.
    for (size_t i = 0; i < 5; ++i) {
      for (size_t j = 0; j < 5; ++j) {
        for (size_t k = 0; k < 64; ++k) {
          uint64_t value = after_theta_rho_pi_state.at(i)[j][k] * 2 +
                           (diluted_mask - after_theta_rho_pi_state.at(i)[(j + 1) % 5][k]) +
                           after_theta_rho_pi_state.at(i)[(j + 2) % 5][k];
          // The condition (~k & (k >> 1)) == 0 is true iff k is of the form 2**m - 1.
          if (i == 0 && j == 0 && (~k & (k >> 1)) == 0 && kRoundKeys.at(kLog.at(k))[round]) {
            value += 2 * diluted_mask;
          }
          state.at(i)[j][k] = diluted_mask & (value >> 1);
          chi_iota_aux0_column_.WriteTrace(
              round + 32 * (RowIndex(i, j, k) + 2048 * component_index), diluted_mask & value,
              trace);
          if (round < 23) {
            state_column_.WriteTrace(
                round + 1 + 32 * (RowIndex(i, j, k) + 2048 * component_index),
                diluted_mask & (value >> 1), trace);
          }
          chi_iota_aux2_column_.WriteTrace(
              round + 32 * (RowIndex(i, j, k) + 2048 * component_index),
              diluted_mask & (value >> 2), trace);
        }
      }
    }
  }
  return input_output;
}

}  // namespace starkware
