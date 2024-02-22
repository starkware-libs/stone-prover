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

#ifndef STARKWARE_AIR_COMPONENTS_KECCAK_KECCAK_H_
#define STARKWARE_AIR_COMPONENTS_KECCAK_KECCAK_H_

#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/diluted_check/diluted_check_cell.h"
#include "starkware/air/components/parse_to_diluted/parse_to_diluted.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/math/math.h"

namespace starkware {

template <typename FieldElementT>
class KeccakComponent {
 public:
  /*
    A component for computing the Keccak hash function.
    See src/starkware/air/components/keccak/keccak.py for documentation.
    The python code is more generic then the C++ implementation, which assumes the following:
    state_rep = [200] * 8
    ell = 6
    u = 5
    alpha = 3
    beta = 2
    rounds = 24.
  */
  KeccakComponent(
      const std::string& name, const TraceGenerationContext& ctx, size_t n_invocations,
      gsl::span<diluted_check_cell::DilutedCheckCell<FieldElementT>*> diluted_pools,
      size_t diluted_spacing)
      : n_invocations_(n_invocations),
        diluted_spacing_(diluted_spacing),
        parse_to_diluted_(
            name + "/parse_to_diluted", ctx, std::vector<size_t>(8, 200),
            std::vector<size_t>{64, 25}, std::vector<size_t>{64, 32}, 2, diluted_spacing,
            n_invocations),
        state_column_(diluted_pools[0], name + "/state", ctx),
        state_begin_column_(diluted_pools[0], name + "/state_begin", ctx),
        state_end_column_(diluted_pools[0], name + "/state_end", ctx),
        parity_columns_{InitParityColumns(name, ctx, diluted_pools)},
        rotated_parity_columns_{ctx.GetVirtualColumn(name + "/rotated_parity0"),
                                ctx.GetVirtualColumn(name + "/rotated_parity1"),
                                ctx.GetVirtualColumn(name + "/rotated_parity2"),
                                ctx.GetVirtualColumn(name + "/rotated_parity3"),
                                ctx.GetVirtualColumn(name + "/rotated_parity4")},
        after_theta_rho_pi_column_(diluted_pools[3], name + "/after_theta_rho_pi", ctx),
        theta_aux_columns_{InitThetaAuxColumns(name, ctx, diluted_pools)},
        chi_iota_aux0_column_(diluted_pools[1], name + "/chi_iota_aux0", ctx),
        chi_iota_aux2_column_(diluted_pools[2], name + "/chi_iota_aux2", ctx) {
    ASSERT_RELEASE(n_invocations_ * diluted_spacing <= 64, "Bits exceed uint64_t.");
  }

  /*
    Computes the row of index [i, j, k] in the state representing virtual column.
  */
  static constexpr size_t RowIndex(size_t i, size_t j, size_t k) { return j + 5 * i + 32 * k; }

  /*
    Writes the trace for one instance of the component.
    input is the concatenation of all inputs of the n_invocations_ invocations. One instance
    includes 200 * n_invocations_ bytes.
    Returns the inputs and the outputs as field elements.
  */
  std::vector<FieldElementT> WriteTrace(
      gsl::span<const std::byte> input, uint64_t component_index,
      gsl::span<const gsl::span<FieldElementT>> trace) const;

  static constexpr size_t kBytesInWord = 25;
  static constexpr size_t kStateSizeInBytes = Keccak256::kStateNumBytes;
  static constexpr size_t kStateSizeInWords = kStateSizeInBytes / kBytesInWord;

 private:
  static constexpr size_t kEll = 6;
  // The inverse function of 2**m-1.
  static constexpr std::array<size_t, 64> kLog{0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4,
                                               4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5,
                                               5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                                               5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6};
  static constexpr std::array<std::array<bool, 24>, 7> kRoundKeys{
      {{true, false, false, false, true,  true,  true,  true,  false, false, true, false,
        true, true,  true,  true,  false, false, false, false, true,  false, true, false},
       {false, true, true,  false, true, false, false, false, true,  false, false, true,
        true,  true, false, true,  true, false, true,  true,  false, false, false, false},
       {false, false, true, false, true,  false, false, true, true,  true,  true,  true,
        true,  true,  true, false, false, false, true,  true, false, false, false, true},
       {false, true, true, false, true,  false, true,  false, true, true, false, false,
        true,  true, true, false, false, true,  false, false, true, true, false, false},
       {false, true,  true, true, true, false, true, true,  false, false, true,  false,
        true,  false, true, true, true, false, true, false, true,  true,  false, true},
       {false, false, false, true,  false, true,  true,  false, false, false, true, true,
        true,  false, false, false, false, false, false, true,  true,  false, true, true},
       {false, false, true, true, false, false, true,  true, false, false, false, false,
        false, true,  true, true, true,  true,  false, true, true,  true,  false, true}}};

  static constexpr std::array<std::array<size_t, 5>, 5> kOffsets{{{0, 1, 62, 28, 27},
                                                                  {36, 44, 6, 55, 20},
                                                                  {3, 10, 43, 25, 39},
                                                                  {41, 45, 15, 21, 8},
                                                                  {18, 2, 61, 56, 14}}};

  static constexpr std::vector<std::vector<TableCheckCellView<FieldElementT>>> InitParityColumns(
      const std::string& name, const TraceGenerationContext& ctx,
      gsl::span<diluted_check_cell::DilutedCheckCell<FieldElementT>*> diluted_pools) {
    std::vector<std::vector<TableCheckCellView<FieldElementT>>> parity_columns;
    parity_columns.reserve(3);
    for (size_t b = 0, pool_index = 0; b < 3; ++b) {
      parity_columns.push_back({});
      parity_columns.back().reserve(5);
      for (size_t j = 0; j < 5; ++j, pool_index = (pool_index + 1) % 3) {
        parity_columns.back().emplace_back(
            diluted_pools[pool_index],
            name + "/parity" + std::to_string(b) + "_" + std::to_string(j), ctx);
      }
    }
    return parity_columns;
  }

  static constexpr std::vector<std::vector<std::vector<TableCheckCellView<FieldElementT>>>>
  InitThetaAuxColumns(
      const std::string& name, const TraceGenerationContext& ctx,
      gsl::span<diluted_check_cell::DilutedCheckCell<FieldElementT>*> diluted_pools) {
    // The pool indices in which the theta_aux columns are allocated.
    constexpr std::array<uint64_t, 21> kThetaAuxPoolIndices = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2,
                                                               3, 0, 1, 2, 3, 3, 3, 3, 3, 3};
    std::vector<std::vector<std::vector<TableCheckCellView<FieldElementT>>>> theta_aux_columns;
    theta_aux_columns.reserve(5);
    for (size_t i = 0, index = 0; i < 5; ++i) {
      theta_aux_columns.push_back({});
      auto& current_column = theta_aux_columns.back();
      current_column.reserve(5);
      for (size_t j = 0; j < 5; ++j, ++index) {
        current_column.push_back({});
        if (index < kThetaAuxPoolIndices.size()) {
          current_column.back().emplace_back(
              diluted_pools[kThetaAuxPoolIndices.at(index)],
              name + "/theta_aux_i" + std::to_string(i) + "_j" + std::to_string(j), ctx);
        } else {
          // A few theta_aux columns did not fit entirely in the allocated space, so each of them
          // was split into 3 parts (and the constraints were split accordingly), instead of
          // allocating a whole new diluted column.
          current_column.back().emplace_back(
              diluted_pools[3],
              name + "/theta_aux_i" + std::to_string(i) + "_j" + std::to_string(j) +
                  "_start0_stop8",
              ctx);
          current_column.back().emplace_back(
              diluted_pools[3],
              name + "/theta_aux_i" + std::to_string(i) + "_j" + std::to_string(j) +
                  "_start8_stop16",
              ctx);
          current_column.back().emplace_back(
              diluted_pools[3],
              name + "/theta_aux_i" + std::to_string(i) + "_j" + std::to_string(j) +
                  "_start16_stop24",
              ctx);
        }
      }
    }
    return theta_aux_columns;
  }

  /*
    Append field elements from src, represented as chunks of 25 bytes, to dst.
  */
  static void AppendBytesToKeccakIO(
      gsl::span<const std::byte> src, std::vector<FieldElementT>* dst);

  /*
    The number of Keccak invocations that are computed in one instance of the component.
  */
  const size_t n_invocations_;

  /*
    The spacing between bits of different invocations.
  */
  const size_t diluted_spacing_;

  /*
    A component that parses the input/output into a sequence of diluted elements.
  */
  const ParseToDilutedComponent<FieldElementT> parse_to_diluted_;

  /*
    The virtual columns.
  */
  const TableCheckCellView<FieldElementT> state_column_;
  const TableCheckCellView<FieldElementT> state_begin_column_;
  const TableCheckCellView<FieldElementT> state_end_column_;
  const std::vector<std::vector<TableCheckCellView<FieldElementT>>> parity_columns_;
  const std::vector<VirtualColumn> rotated_parity_columns_;
  const TableCheckCellView<FieldElementT> after_theta_rho_pi_column_;
  const std::vector<std::vector<std::vector<TableCheckCellView<FieldElementT>>>> theta_aux_columns_;
  const TableCheckCellView<FieldElementT> chi_iota_aux0_column_;
  const TableCheckCellView<FieldElementT> chi_iota_aux2_column_;
};

}  // namespace starkware

#include "starkware/air/components/keccak/keccak.inl"

#endif  // STARKWARE_AIR_COMPONENTS_KECCAK_KECCAK_H_
