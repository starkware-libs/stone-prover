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

#include <map>
#include <utility>

#include "starkware/math/math.h"
#include "starkware/utils/task_manager.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
std::unique_ptr<CompositionPolynomial>
CpuAirDefinition<FieldElementT, 10>::CreateCompositionPolynomial(
    const FieldElement& trace_generator, const ConstFieldElementSpan& random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const FieldElementT& gen = trace_generator.As<FieldElementT>();

  const std::vector<uint64_t> point_exponents = {
      trace_length_, SafeDiv(trace_length_, 2), SafeDiv(trace_length_, 8),
      SafeDiv(trace_length_, 16)};
  const std::vector<uint64_t> gen_exponents = {
      SafeDiv((15) * (trace_length_), 16), (trace_length_) - (1),
      (16) * ((SafeDiv(trace_length_, 16)) - (1)), (2) * ((SafeDiv(trace_length_, 2)) - (1))};

  BuildPeriodicColumns(gen, &builder);

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
CpuAirDefinition<FieldElementT, 10>::PrecomputeDomainEvalsOnCoset(
    const FieldElementT& point, const FieldElementT& generator,
    gsl::span<const uint64_t> point_exponents,
    gsl::span<const FieldElementT> shifts) const {
  const std::vector<FieldElementT> strict_point_powers = BatchPow(point, point_exponents);
  const std::vector<FieldElementT> gen_powers = BatchPow(generator, point_exponents);

  // point_powers[i][j] is the evaluation of the ith power at its jth point.
  // The index j runs until the order of the domain (beyond we'd cycle back to point_powers[i][0]).
  std::vector<std::vector<FieldElementT>> point_powers(point_exponents.size());
  for (size_t i = 0; i < point_exponents.size(); ++i) {
    uint64_t size = SafeDiv(trace_length_, point_exponents[i]);
    auto& vec = point_powers[i];
    auto power = strict_point_powers[i];
    vec.reserve(size);
    vec.push_back(power);
    for (size_t j = 1; j < size; ++j) {
      power *= gen_powers[i];
      vec.push_back(power);
    }
  }

  TaskManager& task_manager = TaskManager::GetInstance();
  constexpr size_t kPeriodUpperBound = 524289;
  constexpr size_t kTaskSize = 1024;
  size_t period;

  std::vector<std::vector<FieldElementT>> precomp_domains = {
      FieldElementT::UninitializedVector(1),  FieldElementT::UninitializedVector(2),
      FieldElementT::UninitializedVector(8),  FieldElementT::UninitializedVector(16),
      FieldElementT::UninitializedVector(16),
  };

  period = 1;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[0][i] = (point_powers[0][i & (0)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 2;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[1][i] = (point_powers[1][i & (1)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 8;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[2][i] = (point_powers[2][i & (7)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 16;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[3][i] = (point_powers[3][i & (15)]) - (shifts[0]);
        }
      },
      period, kTaskSize);

  period = 16;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[4][i] = (point_powers[3][i & (15)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);
  return precomp_domains;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> CpuAirDefinition<FieldElementT, 10>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 4, "shifts should contain 4 elements.");

  // domain0 = point^trace_length - 1.
  const FieldElementT& domain0 = precomp_domains[0];
  // domain1 = point^(trace_length / 2) - 1.
  const FieldElementT& domain1 = precomp_domains[1];
  // domain2 = point^(trace_length / 8) - 1.
  const FieldElementT& domain2 = precomp_domains[2];
  // domain3 = point^(trace_length / 16) - gen^(15 * trace_length / 16).
  const FieldElementT& domain3 = precomp_domains[3];
  // domain4 = point^(trace_length / 16) - 1.
  const FieldElementT& domain4 = precomp_domains[4];
  // domain5 = point - gen^(trace_length - 1).
  const FieldElementT& domain5 = (point) - (shifts[1]);
  // domain6 = point - gen^(16 * (trace_length / 16 - 1)).
  const FieldElementT& domain6 = (point) - (shifts[2]);
  // domain7 = point - 1.
  const FieldElementT& domain7 = (point) - (FieldElementT::One());
  // domain8 = point - gen^(2 * (trace_length / 2 - 1)).
  const FieldElementT& domain8 = (point) - (shifts[3]);

  ASSERT_VERIFIER(neighbors.size() == 49, "Neighbors must contain 49 elements.");
  const FieldElementT& column0_row0 = neighbors[kColumn0Row0Neighbor];
  const FieldElementT& column0_row1 = neighbors[kColumn0Row1Neighbor];
  const FieldElementT& column0_row4 = neighbors[kColumn0Row4Neighbor];
  const FieldElementT& column0_row8 = neighbors[kColumn0Row8Neighbor];
  const FieldElementT& column1_row0 = neighbors[kColumn1Row0Neighbor];
  const FieldElementT& column1_row1 = neighbors[kColumn1Row1Neighbor];
  const FieldElementT& column1_row2 = neighbors[kColumn1Row2Neighbor];
  const FieldElementT& column1_row3 = neighbors[kColumn1Row3Neighbor];
  const FieldElementT& column1_row4 = neighbors[kColumn1Row4Neighbor];
  const FieldElementT& column1_row5 = neighbors[kColumn1Row5Neighbor];
  const FieldElementT& column1_row6 = neighbors[kColumn1Row6Neighbor];
  const FieldElementT& column1_row7 = neighbors[kColumn1Row7Neighbor];
  const FieldElementT& column1_row8 = neighbors[kColumn1Row8Neighbor];
  const FieldElementT& column1_row9 = neighbors[kColumn1Row9Neighbor];
  const FieldElementT& column1_row10 = neighbors[kColumn1Row10Neighbor];
  const FieldElementT& column1_row11 = neighbors[kColumn1Row11Neighbor];
  const FieldElementT& column1_row12 = neighbors[kColumn1Row12Neighbor];
  const FieldElementT& column1_row13 = neighbors[kColumn1Row13Neighbor];
  const FieldElementT& column1_row14 = neighbors[kColumn1Row14Neighbor];
  const FieldElementT& column1_row15 = neighbors[kColumn1Row15Neighbor];
  const FieldElementT& column2_row0 = neighbors[kColumn2Row0Neighbor];
  const FieldElementT& column2_row1 = neighbors[kColumn2Row1Neighbor];
  const FieldElementT& column3_row0 = neighbors[kColumn3Row0Neighbor];
  const FieldElementT& column3_row1 = neighbors[kColumn3Row1Neighbor];
  const FieldElementT& column3_row2 = neighbors[kColumn3Row2Neighbor];
  const FieldElementT& column3_row3 = neighbors[kColumn3Row3Neighbor];
  const FieldElementT& column3_row4 = neighbors[kColumn3Row4Neighbor];
  const FieldElementT& column3_row5 = neighbors[kColumn3Row5Neighbor];
  const FieldElementT& column3_row8 = neighbors[kColumn3Row8Neighbor];
  const FieldElementT& column3_row9 = neighbors[kColumn3Row9Neighbor];
  const FieldElementT& column3_row12 = neighbors[kColumn3Row12Neighbor];
  const FieldElementT& column3_row13 = neighbors[kColumn3Row13Neighbor];
  const FieldElementT& column3_row16 = neighbors[kColumn3Row16Neighbor];
  const FieldElementT& column4_row0 = neighbors[kColumn4Row0Neighbor];
  const FieldElementT& column4_row1 = neighbors[kColumn4Row1Neighbor];
  const FieldElementT& column4_row2 = neighbors[kColumn4Row2Neighbor];
  const FieldElementT& column4_row3 = neighbors[kColumn4Row3Neighbor];
  const FieldElementT& column5_row0 = neighbors[kColumn5Row0Neighbor];
  const FieldElementT& column5_row2 = neighbors[kColumn5Row2Neighbor];
  const FieldElementT& column5_row4 = neighbors[kColumn5Row4Neighbor];
  const FieldElementT& column5_row8 = neighbors[kColumn5Row8Neighbor];
  const FieldElementT& column5_row10 = neighbors[kColumn5Row10Neighbor];
  const FieldElementT& column5_row12 = neighbors[kColumn5Row12Neighbor];
  const FieldElementT& column5_row16 = neighbors[kColumn5Row16Neighbor];
  const FieldElementT& column5_row24 = neighbors[kColumn5Row24Neighbor];
  const FieldElementT& column6_inter1_row0 = neighbors[kColumn6Inter1Row0Neighbor];
  const FieldElementT& column6_inter1_row1 = neighbors[kColumn6Inter1Row1Neighbor];
  const FieldElementT& column7_inter1_row0 = neighbors[kColumn7Inter1Row0Neighbor];
  const FieldElementT& column7_inter1_row2 = neighbors[kColumn7Inter1Row2Neighbor];

  ASSERT_VERIFIER(periodic_columns.empty(), "periodic_columns should be empty.");

  const FieldElementT cpu__decode__opcode_rc__bit_0 =
      (column1_row0) - ((column1_row1) + (column1_row1));
  const FieldElementT cpu__decode__opcode_rc__bit_2 =
      (column1_row2) - ((column1_row3) + (column1_row3));
  const FieldElementT cpu__decode__opcode_rc__bit_4 =
      (column1_row4) - ((column1_row5) + (column1_row5));
  const FieldElementT cpu__decode__opcode_rc__bit_3 =
      (column1_row3) - ((column1_row4) + (column1_row4));
  const FieldElementT cpu__decode__flag_op1_base_op0_0 =
      (FieldElementT::One()) -
      (((cpu__decode__opcode_rc__bit_2) + (cpu__decode__opcode_rc__bit_4)) +
       (cpu__decode__opcode_rc__bit_3));
  const FieldElementT cpu__decode__opcode_rc__bit_5 =
      (column1_row5) - ((column1_row6) + (column1_row6));
  const FieldElementT cpu__decode__opcode_rc__bit_6 =
      (column1_row6) - ((column1_row7) + (column1_row7));
  const FieldElementT cpu__decode__opcode_rc__bit_9 =
      (column1_row9) - ((column1_row10) + (column1_row10));
  const FieldElementT cpu__decode__flag_res_op1_0 =
      (FieldElementT::One()) -
      (((cpu__decode__opcode_rc__bit_5) + (cpu__decode__opcode_rc__bit_6)) +
       (cpu__decode__opcode_rc__bit_9));
  const FieldElementT cpu__decode__opcode_rc__bit_7 =
      (column1_row7) - ((column1_row8) + (column1_row8));
  const FieldElementT cpu__decode__opcode_rc__bit_8 =
      (column1_row8) - ((column1_row9) + (column1_row9));
  const FieldElementT cpu__decode__flag_pc_update_regular_0 =
      (FieldElementT::One()) -
      (((cpu__decode__opcode_rc__bit_7) + (cpu__decode__opcode_rc__bit_8)) +
       (cpu__decode__opcode_rc__bit_9));
  const FieldElementT cpu__decode__opcode_rc__bit_12 =
      (column1_row12) - ((column1_row13) + (column1_row13));
  const FieldElementT cpu__decode__opcode_rc__bit_13 =
      (column1_row13) - ((column1_row14) + (column1_row14));
  const FieldElementT cpu__decode__fp_update_regular_0 =
      (FieldElementT::One()) -
      ((cpu__decode__opcode_rc__bit_12) + (cpu__decode__opcode_rc__bit_13));
  const FieldElementT cpu__decode__opcode_rc__bit_1 =
      (column1_row1) - ((column1_row2) + (column1_row2));
  const FieldElementT npc_reg_0 =
      ((column3_row0) + (cpu__decode__opcode_rc__bit_2)) + (FieldElementT::One());
  const FieldElementT cpu__decode__opcode_rc__bit_10 =
      (column1_row10) - ((column1_row11) + (column1_row11));
  const FieldElementT cpu__decode__opcode_rc__bit_11 =
      (column1_row11) - ((column1_row12) + (column1_row12));
  const FieldElementT cpu__decode__opcode_rc__bit_14 =
      (column1_row14) - ((column1_row15) + (column1_row15));
  const FieldElementT memory__address_diff_0 = (column4_row2) - (column4_row0);
  const FieldElementT rc16__diff_0 = (column2_row1) - (column2_row0);
  FractionFieldElement<FieldElementT> res(FieldElementT::Zero());
  {
    // Compute a sum of constraints with denominator = domain0.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain3.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/decode/opcode_rc/bit:
        const FieldElementT constraint =
            ((cpu__decode__opcode_rc__bit_0) * (cpu__decode__opcode_rc__bit_0)) -
            (cpu__decode__opcode_rc__bit_0);
        inner_sum += random_coefficients[0] * constraint;
      }
      outer_sum += inner_sum * domain3;
    }

    {
      // Compute a sum of constraints with numerator = domain5.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc16/perm/step0:
        const FieldElementT constraint =
            (((rc16__perm__interaction_elm_) - (column2_row1)) * (column6_inter1_row1)) -
            (((rc16__perm__interaction_elm_) - (column0_row1)) * (column6_inter1_row0));
        inner_sum += random_coefficients[42] * constraint;
      }
      {
        // Constraint expression for rc16/diff_is_bit:
        const FieldElementT constraint = ((rc16__diff_0) * (rc16__diff_0)) - (rc16__diff_0);
        inner_sum += random_coefficients[44] * constraint;
      }
      outer_sum += inner_sum * domain5;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain0);
  }

  {
    // Compute a sum of constraints with denominator = domain3.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/decode/opcode_rc/zero:
        const FieldElementT constraint = column1_row0;
        inner_sum += random_coefficients[1] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain3);
  }

  {
    // Compute a sum of constraints with denominator = domain4.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/decode/opcode_rc_input:
        const FieldElementT constraint =
            (column3_row1) -
            (((((((column1_row0) * (offset_size_)) + (column0_row4)) * (offset_size_)) +
               (column0_row8)) *
              (offset_size_)) +
             (column0_row0));
        inner_sum += random_coefficients[2] * constraint;
      }
      {
        // Constraint expression for cpu/decode/flag_op1_base_op0_bit:
        const FieldElementT constraint =
            ((cpu__decode__flag_op1_base_op0_0) * (cpu__decode__flag_op1_base_op0_0)) -
            (cpu__decode__flag_op1_base_op0_0);
        inner_sum += random_coefficients[3] * constraint;
      }
      {
        // Constraint expression for cpu/decode/flag_res_op1_bit:
        const FieldElementT constraint =
            ((cpu__decode__flag_res_op1_0) * (cpu__decode__flag_res_op1_0)) -
            (cpu__decode__flag_res_op1_0);
        inner_sum += random_coefficients[4] * constraint;
      }
      {
        // Constraint expression for cpu/decode/flag_pc_update_regular_bit:
        const FieldElementT constraint =
            ((cpu__decode__flag_pc_update_regular_0) * (cpu__decode__flag_pc_update_regular_0)) -
            (cpu__decode__flag_pc_update_regular_0);
        inner_sum += random_coefficients[5] * constraint;
      }
      {
        // Constraint expression for cpu/decode/fp_update_regular_bit:
        const FieldElementT constraint =
            ((cpu__decode__fp_update_regular_0) * (cpu__decode__fp_update_regular_0)) -
            (cpu__decode__fp_update_regular_0);
        inner_sum += random_coefficients[6] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem_dst_addr:
        const FieldElementT constraint =
            ((column3_row8) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_0) * (column5_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_0)) * (column5_row0))) +
             (column0_row0));
        inner_sum += random_coefficients[7] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem0_addr:
        const FieldElementT constraint =
            ((column3_row4) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_1) * (column5_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_1)) * (column5_row0))) +
             (column0_row8));
        inner_sum += random_coefficients[8] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem1_addr:
        const FieldElementT constraint = ((column3_row12) + (half_offset_size_)) -
                                         ((((((cpu__decode__opcode_rc__bit_2) * (column3_row0)) +
                                             ((cpu__decode__opcode_rc__bit_4) * (column5_row0))) +
                                            ((cpu__decode__opcode_rc__bit_3) * (column5_row8))) +
                                           ((cpu__decode__flag_op1_base_op0_0) * (column3_row5))) +
                                          (column0_row4));
        inner_sum += random_coefficients[9] * constraint;
      }
      {
        // Constraint expression for cpu/operands/ops_mul:
        const FieldElementT constraint = (column5_row4) - ((column3_row5) * (column3_row13));
        inner_sum += random_coefficients[10] * constraint;
      }
      {
        // Constraint expression for cpu/operands/res:
        const FieldElementT constraint =
            (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column5_row12)) -
            ((((cpu__decode__opcode_rc__bit_5) * ((column3_row5) + (column3_row13))) +
              ((cpu__decode__opcode_rc__bit_6) * (column5_row4))) +
             ((cpu__decode__flag_res_op1_0) * (column3_row13)));
        inner_sum += random_coefficients[11] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_fp:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column3_row9) - (column5_row8));
        inner_sum += random_coefficients[18] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_pc:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) *
            ((column3_row5) -
             (((column3_row0) + (cpu__decode__opcode_rc__bit_2)) + (FieldElementT::One())));
        inner_sum += random_coefficients[19] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off0:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column0_row0) - (half_offset_size_));
        inner_sum += random_coefficients[20] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off1:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) *
            ((column0_row8) - ((half_offset_size_) + (FieldElementT::One())));
        inner_sum += random_coefficients[21] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/flags:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) *
            (((((cpu__decode__opcode_rc__bit_12) + (cpu__decode__opcode_rc__bit_12)) +
               (FieldElementT::One())) +
              (FieldElementT::One())) -
             (((cpu__decode__opcode_rc__bit_0) + (cpu__decode__opcode_rc__bit_1)) +
              (FieldElementT::ConstexprFromBigInt(0x4_Z))));
        inner_sum += random_coefficients[22] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/off0:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_13) *
            (((column0_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z))) - (half_offset_size_));
        inner_sum += random_coefficients[23] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/off2:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_13) *
            (((column0_row4) + (FieldElementT::One())) - (half_offset_size_));
        inner_sum += random_coefficients[24] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/flags:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_13) *
            (((((cpu__decode__opcode_rc__bit_7) + (cpu__decode__opcode_rc__bit_0)) +
               (cpu__decode__opcode_rc__bit_3)) +
              (cpu__decode__flag_res_op1_0)) -
             (FieldElementT::ConstexprFromBigInt(0x4_Z)));
        inner_sum += random_coefficients[25] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/assert_eq/assert_eq:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_14) * ((column3_row9) - (column5_row12));
        inner_sum += random_coefficients[26] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain6.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp0:
        const FieldElementT constraint =
            (column5_row2) - ((cpu__decode__opcode_rc__bit_9) * (column3_row9));
        inner_sum += random_coefficients[12] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp1:
        const FieldElementT constraint = (column5_row10) - ((column5_row2) * (column5_row12));
        inner_sum += random_coefficients[13] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_negative:
        const FieldElementT constraint =
            ((((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column3_row16)) +
             ((column5_row2) * ((column3_row16) - ((column3_row0) + (column3_row13))))) -
            ((((cpu__decode__flag_pc_update_regular_0) * (npc_reg_0)) +
              ((cpu__decode__opcode_rc__bit_7) * (column5_row12))) +
             ((cpu__decode__opcode_rc__bit_8) * ((column3_row0) + (column5_row12))));
        inner_sum += random_coefficients[14] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_positive:
        const FieldElementT constraint =
            ((column5_row10) - (cpu__decode__opcode_rc__bit_9)) * ((column3_row16) - (npc_reg_0));
        inner_sum += random_coefficients[15] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_ap/ap_update:
        const FieldElementT constraint =
            (column5_row16) -
            ((((column5_row0) + ((cpu__decode__opcode_rc__bit_10) * (column5_row12))) +
              (cpu__decode__opcode_rc__bit_11)) +
             ((cpu__decode__opcode_rc__bit_12) * (FieldElementT::ConstexprFromBigInt(0x2_Z))));
        inner_sum += random_coefficients[16] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_fp/fp_update:
        const FieldElementT constraint =
            (column5_row24) - ((((cpu__decode__fp_update_regular_0) * (column5_row8)) +
                                ((cpu__decode__opcode_rc__bit_13) * (column3_row9))) +
                               ((cpu__decode__opcode_rc__bit_12) *
                                ((column5_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z)))));
        inner_sum += random_coefficients[17] * constraint;
      }
      outer_sum += inner_sum * domain6;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain4);
  }

  {
    // Compute a sum of constraints with denominator = domain7.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for initial_ap:
        const FieldElementT constraint = (column5_row0) - (initial_ap_);
        inner_sum += random_coefficients[27] * constraint;
      }
      {
        // Constraint expression for initial_fp:
        const FieldElementT constraint = (column5_row8) - (initial_ap_);
        inner_sum += random_coefficients[28] * constraint;
      }
      {
        // Constraint expression for initial_pc:
        const FieldElementT constraint = (column3_row0) - (initial_pc_);
        inner_sum += random_coefficients[29] * constraint;
      }
      {
        // Constraint expression for memory/multi_column_perm/perm/init0:
        const FieldElementT constraint =
            (((((memory__multi_column_perm__perm__interaction_elm_) -
                ((column4_row0) +
                 ((memory__multi_column_perm__hash_interaction_elm0_) * (column4_row1)))) *
               (column7_inter1_row0)) +
              (column3_row0)) +
             ((memory__multi_column_perm__hash_interaction_elm0_) * (column3_row1))) -
            (memory__multi_column_perm__perm__interaction_elm_);
        inner_sum += random_coefficients[33] * constraint;
      }
      {
        // Constraint expression for memory/initial_addr:
        const FieldElementT constraint = (column4_row0) - (FieldElementT::One());
        inner_sum += random_coefficients[38] * constraint;
      }
      {
        // Constraint expression for rc16/perm/init0:
        const FieldElementT constraint =
            ((((rc16__perm__interaction_elm_) - (column2_row0)) * (column6_inter1_row0)) +
             (column0_row0)) -
            (rc16__perm__interaction_elm_);
        inner_sum += random_coefficients[41] * constraint;
      }
      {
        // Constraint expression for rc16/minimum:
        const FieldElementT constraint = (column2_row0) - (rc_min_);
        inner_sum += random_coefficients[45] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain7);
  }

  {
    // Compute a sum of constraints with denominator = domain6.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for final_ap:
        const FieldElementT constraint = (column5_row0) - (final_ap_);
        inner_sum += random_coefficients[30] * constraint;
      }
      {
        // Constraint expression for final_fp:
        const FieldElementT constraint = (column5_row8) - (initial_ap_);
        inner_sum += random_coefficients[31] * constraint;
      }
      {
        // Constraint expression for final_pc:
        const FieldElementT constraint = (column3_row0) - (final_pc_);
        inner_sum += random_coefficients[32] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain6);
  }

  {
    // Compute a sum of constraints with denominator = domain1.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain8.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/step0:
        const FieldElementT constraint =
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column4_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column4_row3)))) *
             (column7_inter1_row2)) -
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column3_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column3_row3)))) *
             (column7_inter1_row0));
        inner_sum += random_coefficients[34] * constraint;
      }
      {
        // Constraint expression for memory/diff_is_bit:
        const FieldElementT constraint =
            ((memory__address_diff_0) * (memory__address_diff_0)) - (memory__address_diff_0);
        inner_sum += random_coefficients[36] * constraint;
      }
      {
        // Constraint expression for memory/is_func:
        const FieldElementT constraint =
            ((memory__address_diff_0) - (FieldElementT::One())) * ((column4_row1) - (column4_row3));
        inner_sum += random_coefficients[37] * constraint;
      }
      outer_sum += inner_sum * domain8;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain1);
  }

  {
    // Compute a sum of constraints with denominator = domain8.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/last:
        const FieldElementT constraint =
            (column7_inter1_row0) - (memory__multi_column_perm__perm__public_memory_prod_);
        inner_sum += random_coefficients[35] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain8);
  }

  {
    // Compute a sum of constraints with denominator = domain2.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for public_memory_addr_zero:
        const FieldElementT constraint = column3_row2;
        inner_sum += random_coefficients[39] * constraint;
      }
      {
        // Constraint expression for public_memory_value_zero:
        const FieldElementT constraint = column3_row3;
        inner_sum += random_coefficients[40] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain2);
  }

  {
    // Compute a sum of constraints with denominator = domain5.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc16/perm/last:
        const FieldElementT constraint = (column6_inter1_row0) - (rc16__perm__public_memory_prod_);
        inner_sum += random_coefficients[43] * constraint;
      }
      {
        // Constraint expression for rc16/maximum:
        const FieldElementT constraint = (column2_row0) - (rc_max_);
        inner_sum += random_coefficients[46] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain5);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> CpuAirDefinition<FieldElementT, 10>::DomainEvalsAtPoint(
    gsl::span<const FieldElementT> point_powers,
    gsl::span<const FieldElementT> shifts) const {
  const FieldElementT& domain0 = (point_powers[1]) - (FieldElementT::One());
  const FieldElementT& domain1 = (point_powers[2]) - (FieldElementT::One());
  const FieldElementT& domain2 = (point_powers[3]) - (FieldElementT::One());
  const FieldElementT& domain3 = (point_powers[4]) - (shifts[0]);
  const FieldElementT& domain4 = (point_powers[4]) - (FieldElementT::One());
  return {
      domain0, domain1, domain2, domain3, domain4,
  };
}

template <typename FieldElementT>
TraceGenerationContext CpuAirDefinition<FieldElementT, 10>::GetTraceGenerationContext() const {
  TraceGenerationContext ctx;

  ASSERT_RELEASE(((trace_length_) + (-1)) < (trace_length_), "Index out of range.");

  ASSERT_RELEASE(((trace_length_) + (-1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (trace_length_), "Index out of range.");

  ASSERT_RELEASE((1) <= (trace_length_), "step must not exceed dimension.");

  ASSERT_RELEASE(((trace_length_) - (1)) <= (trace_length_), "Index out of range.");

  ASSERT_RELEASE(((trace_length_) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (trace_length_), "Index out of range.");

  ASSERT_RELEASE((0) <= ((trace_length_) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE((trace_length_) <= (trace_length_), "Index out of range.");

  ASSERT_RELEASE((trace_length_) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 8)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 2)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 2)), "Index out of range.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 2)), "step must not exceed dimension.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 2)) <= (SafeDiv(trace_length_, 2)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 2)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 2)) - (1)) <= (SafeDiv(trace_length_, 2)), "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 2)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 2)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 2)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 2)) + (-1)) < (SafeDiv(trace_length_, 2)), "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 2)) + (-1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 16)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 16)) + (-1)) < (SafeDiv(trace_length_, 16)), "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 16)) + (-1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 16)), "Index out of range.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 16)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 16)) <= (SafeDiv(trace_length_, 16)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 16)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 16)) - (1)) <= (SafeDiv(trace_length_, 16)), "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 16)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 16)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 16)) - (1)), "start must not exceed stop.");

  ctx.AddVirtualColumn(
      "rc16_pool", VirtualColumn(/*column=*/kColumn0Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/opcode_rc/column",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "rc16/sorted", VirtualColumn(/*column=*/kColumn2Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "mem_pool/addr", VirtualColumn(/*column=*/kColumn3Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "mem_pool/value", VirtualColumn(/*column=*/kColumn3Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "memory/sorted/addr", VirtualColumn(/*column=*/kColumn4Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/sorted/value",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/registers/ap", VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/registers/fp", VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/operands/ops_mul",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/res", VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp0",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp1",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/10));
  ctx.AddVirtualColumn(
      "rc16/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn6Inter1Column - kNumColumnsFirst, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/multi_column_perm/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn7Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/mem_inst/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/mem_inst/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/pc", VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/instruction",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/off0", VirtualColumn(/*column=*/kColumn0Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/off1", VirtualColumn(/*column=*/kColumn0Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/decode/off2", VirtualColumn(/*column=*/kColumn0Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_dst/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_dst/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op0/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op0/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op1/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op1/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "orig/public_memory/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/8, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "orig/public_memory/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/8, /*row_offset=*/3));

  return ctx;
}

template <typename FieldElementT>
std::vector<std::pair<int64_t, uint64_t>> CpuAirDefinition<FieldElementT, 10>::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;

  mask.reserve(49);
  mask.emplace_back(0, kColumn0Column);
  mask.emplace_back(1, kColumn0Column);
  mask.emplace_back(4, kColumn0Column);
  mask.emplace_back(8, kColumn0Column);
  mask.emplace_back(0, kColumn1Column);
  mask.emplace_back(1, kColumn1Column);
  mask.emplace_back(2, kColumn1Column);
  mask.emplace_back(3, kColumn1Column);
  mask.emplace_back(4, kColumn1Column);
  mask.emplace_back(5, kColumn1Column);
  mask.emplace_back(6, kColumn1Column);
  mask.emplace_back(7, kColumn1Column);
  mask.emplace_back(8, kColumn1Column);
  mask.emplace_back(9, kColumn1Column);
  mask.emplace_back(10, kColumn1Column);
  mask.emplace_back(11, kColumn1Column);
  mask.emplace_back(12, kColumn1Column);
  mask.emplace_back(13, kColumn1Column);
  mask.emplace_back(14, kColumn1Column);
  mask.emplace_back(15, kColumn1Column);
  mask.emplace_back(0, kColumn2Column);
  mask.emplace_back(1, kColumn2Column);
  mask.emplace_back(0, kColumn3Column);
  mask.emplace_back(1, kColumn3Column);
  mask.emplace_back(2, kColumn3Column);
  mask.emplace_back(3, kColumn3Column);
  mask.emplace_back(4, kColumn3Column);
  mask.emplace_back(5, kColumn3Column);
  mask.emplace_back(8, kColumn3Column);
  mask.emplace_back(9, kColumn3Column);
  mask.emplace_back(12, kColumn3Column);
  mask.emplace_back(13, kColumn3Column);
  mask.emplace_back(16, kColumn3Column);
  mask.emplace_back(0, kColumn4Column);
  mask.emplace_back(1, kColumn4Column);
  mask.emplace_back(2, kColumn4Column);
  mask.emplace_back(3, kColumn4Column);
  mask.emplace_back(0, kColumn5Column);
  mask.emplace_back(2, kColumn5Column);
  mask.emplace_back(4, kColumn5Column);
  mask.emplace_back(8, kColumn5Column);
  mask.emplace_back(10, kColumn5Column);
  mask.emplace_back(12, kColumn5Column);
  mask.emplace_back(16, kColumn5Column);
  mask.emplace_back(24, kColumn5Column);
  mask.emplace_back(0, kColumn6Inter1Column);
  mask.emplace_back(1, kColumn6Inter1Column);
  mask.emplace_back(0, kColumn7Inter1Column);
  mask.emplace_back(2, kColumn7Inter1Column);

  return mask;
}

}  // namespace cpu

}  // namespace starkware
