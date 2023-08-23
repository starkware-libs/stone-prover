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
CpuAirDefinition<FieldElementT, 4>::CreateCompositionPolynomial(
    const FieldElement& trace_generator, const ConstFieldElementSpan& random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const FieldElementT& gen = trace_generator.As<FieldElementT>();

  const std::vector<uint64_t> point_exponents = {
      trace_length_,
      SafeDiv(trace_length_, 2),
      SafeDiv(trace_length_, 4),
      SafeDiv(trace_length_, 16),
      SafeDiv(trace_length_, 32),
      SafeDiv(trace_length_, 128),
      SafeDiv(trace_length_, 1024),
      SafeDiv(trace_length_, 2048)};
  const std::vector<uint64_t> gen_exponents = {
      SafeDiv((15) * (trace_length_), 16),
      SafeDiv((3) * (trace_length_), 4),
      SafeDiv(trace_length_, 64),
      SafeDiv(trace_length_, 32),
      SafeDiv((3) * (trace_length_), 64),
      SafeDiv(trace_length_, 16),
      SafeDiv((5) * (trace_length_), 64),
      SafeDiv((3) * (trace_length_), 32),
      SafeDiv((7) * (trace_length_), 64),
      SafeDiv(trace_length_, 8),
      SafeDiv((9) * (trace_length_), 64),
      SafeDiv((5) * (trace_length_), 32),
      SafeDiv((11) * (trace_length_), 64),
      SafeDiv((3) * (trace_length_), 16),
      SafeDiv((13) * (trace_length_), 64),
      SafeDiv((7) * (trace_length_), 32),
      SafeDiv((15) * (trace_length_), 64),
      SafeDiv((255) * (trace_length_), 256),
      SafeDiv((63) * (trace_length_), 64),
      SafeDiv(trace_length_, 2),
      (trace_length_) - (1),
      (16) * ((SafeDiv(trace_length_, 16)) - (1)),
      (2) * ((SafeDiv(trace_length_, 2)) - (1)),
      (4) * ((SafeDiv(trace_length_, 4)) - (1)),
      (2048) * ((SafeDiv(trace_length_, 2048)) - (1)),
      (128) * ((SafeDiv(trace_length_, 128)) - (1))};

  BuildPeriodicColumns(gen, &builder);

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
CpuAirDefinition<FieldElementT, 4>::PrecomputeDomainEvalsOnCoset(
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
      FieldElementT::UninitializedVector(1),    FieldElementT::UninitializedVector(2),
      FieldElementT::UninitializedVector(4),    FieldElementT::UninitializedVector(16),
      FieldElementT::UninitializedVector(16),   FieldElementT::UninitializedVector(32),
      FieldElementT::UninitializedVector(128),  FieldElementT::UninitializedVector(128),
      FieldElementT::UninitializedVector(128),  FieldElementT::UninitializedVector(1024),
      FieldElementT::UninitializedVector(1024), FieldElementT::UninitializedVector(1024),
      FieldElementT::UninitializedVector(2048), FieldElementT::UninitializedVector(2048),
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

  period = 4;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[2][i] = (point_powers[2][i & (3)]) - (FieldElementT::One());
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

  period = 32;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[5][i] = (point_powers[4][i & (31)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[6][i] = (point_powers[5][i & (127)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[7][i] = (point_powers[5][i & (127)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[8][i] = ((point_powers[5][i & (127)]) - (shifts[2])) *
                                  ((point_powers[5][i & (127)]) - (shifts[3])) *
                                  ((point_powers[5][i & (127)]) - (shifts[4])) *
                                  ((point_powers[5][i & (127)]) - (shifts[5])) *
                                  ((point_powers[5][i & (127)]) - (shifts[6])) *
                                  ((point_powers[5][i & (127)]) - (shifts[7])) *
                                  ((point_powers[5][i & (127)]) - (shifts[8])) *
                                  ((point_powers[5][i & (127)]) - (shifts[9])) *
                                  ((point_powers[5][i & (127)]) - (shifts[10])) *
                                  ((point_powers[5][i & (127)]) - (shifts[11])) *
                                  ((point_powers[5][i & (127)]) - (shifts[12])) *
                                  ((point_powers[5][i & (127)]) - (shifts[13])) *
                                  ((point_powers[5][i & (127)]) - (shifts[14])) *
                                  ((point_powers[5][i & (127)]) - (shifts[15])) *
                                  ((point_powers[5][i & (127)]) - (shifts[16])) *
                                  (precomp_domains[6][i & (128 - 1)]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[9][i] = (point_powers[6][i & (1023)]) - (shifts[17]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[10][i] = (point_powers[6][i & (1023)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[11][i] = (point_powers[6][i & (1023)]) - (shifts[18]);
        }
      },
      period, kTaskSize);

  period = 2048;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[12][i] = (point_powers[7][i & (2047)]) - (shifts[19]);
        }
      },
      period, kTaskSize);

  period = 2048;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[13][i] = (point_powers[7][i & (2047)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);
  return precomp_domains;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> CpuAirDefinition<FieldElementT, 4>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 26, "shifts should contain 26 elements.");

  // domain0 = point^trace_length - 1.
  const FieldElementT& domain0 = precomp_domains[0];
  // domain1 = point^(trace_length / 2) - 1.
  const FieldElementT& domain1 = precomp_domains[1];
  // domain2 = point^(trace_length / 4) - 1.
  const FieldElementT& domain2 = precomp_domains[2];
  // domain3 = point^(trace_length / 16) - gen^(15 * trace_length / 16).
  const FieldElementT& domain3 = precomp_domains[3];
  // domain4 = point^(trace_length / 16) - 1.
  const FieldElementT& domain4 = precomp_domains[4];
  // domain5 = point^(trace_length / 32) - 1.
  const FieldElementT& domain5 = precomp_domains[5];
  // domain6 = point^(trace_length / 128) - 1.
  const FieldElementT& domain6 = precomp_domains[6];
  // domain7 = point^(trace_length / 128) - gen^(3 * trace_length / 4).
  const FieldElementT& domain7 = precomp_domains[7];
  // domain8 = (point^(trace_length / 128) - gen^(trace_length / 64)) * (point^(trace_length / 128)
  // - gen^(trace_length / 32)) * (point^(trace_length / 128) - gen^(3 * trace_length / 64)) *
  // (point^(trace_length / 128) - gen^(trace_length / 16)) * (point^(trace_length / 128) - gen^(5 *
  // trace_length / 64)) * (point^(trace_length / 128) - gen^(3 * trace_length / 32)) *
  // (point^(trace_length / 128) - gen^(7 * trace_length / 64)) * (point^(trace_length / 128) -
  // gen^(trace_length / 8)) * (point^(trace_length / 128) - gen^(9 * trace_length / 64)) *
  // (point^(trace_length / 128) - gen^(5 * trace_length / 32)) * (point^(trace_length / 128) -
  // gen^(11 * trace_length / 64)) * (point^(trace_length / 128) - gen^(3 * trace_length / 16)) *
  // (point^(trace_length / 128) - gen^(13 * trace_length / 64)) * (point^(trace_length / 128) -
  // gen^(7 * trace_length / 32)) * (point^(trace_length / 128) - gen^(15 * trace_length / 64)) *
  // domain6.
  const FieldElementT& domain8 = precomp_domains[8];
  // domain9 = point^(trace_length / 1024) - gen^(255 * trace_length / 256).
  const FieldElementT& domain9 = precomp_domains[9];
  // domain10 = point^(trace_length / 1024) - 1.
  const FieldElementT& domain10 = precomp_domains[10];
  // domain11 = point^(trace_length / 1024) - gen^(63 * trace_length / 64).
  const FieldElementT& domain11 = precomp_domains[11];
  // domain12 = point^(trace_length / 2048) - gen^(trace_length / 2).
  const FieldElementT& domain12 = precomp_domains[12];
  // domain13 = point^(trace_length / 2048) - 1.
  const FieldElementT& domain13 = precomp_domains[13];
  // domain14 = point - gen^(trace_length - 1).
  const FieldElementT& domain14 = (point) - (shifts[20]);
  // domain15 = point - gen^(16 * (trace_length / 16 - 1)).
  const FieldElementT& domain15 = (point) - (shifts[21]);
  // domain16 = point - 1.
  const FieldElementT& domain16 = (point) - (FieldElementT::One());
  // domain17 = point - gen^(2 * (trace_length / 2 - 1)).
  const FieldElementT& domain17 = (point) - (shifts[22]);
  // domain18 = point - gen^(4 * (trace_length / 4 - 1)).
  const FieldElementT& domain18 = (point) - (shifts[23]);
  // domain19 = point - gen^(2048 * (trace_length / 2048 - 1)).
  const FieldElementT& domain19 = (point) - (shifts[24]);
  // domain20 = point - gen^(128 * (trace_length / 128 - 1)).
  const FieldElementT& domain20 = (point) - (shifts[25]);

  ASSERT_VERIFIER(neighbors.size() == 133, "Neighbors must contain 133 elements.");
  const FieldElementT& column0_row0 = neighbors[kColumn0Row0Neighbor];
  const FieldElementT& column0_row1 = neighbors[kColumn0Row1Neighbor];
  const FieldElementT& column0_row2 = neighbors[kColumn0Row2Neighbor];
  const FieldElementT& column0_row3 = neighbors[kColumn0Row3Neighbor];
  const FieldElementT& column0_row4 = neighbors[kColumn0Row4Neighbor];
  const FieldElementT& column0_row5 = neighbors[kColumn0Row5Neighbor];
  const FieldElementT& column0_row6 = neighbors[kColumn0Row6Neighbor];
  const FieldElementT& column0_row7 = neighbors[kColumn0Row7Neighbor];
  const FieldElementT& column0_row8 = neighbors[kColumn0Row8Neighbor];
  const FieldElementT& column0_row9 = neighbors[kColumn0Row9Neighbor];
  const FieldElementT& column0_row10 = neighbors[kColumn0Row10Neighbor];
  const FieldElementT& column0_row11 = neighbors[kColumn0Row11Neighbor];
  const FieldElementT& column0_row12 = neighbors[kColumn0Row12Neighbor];
  const FieldElementT& column0_row13 = neighbors[kColumn0Row13Neighbor];
  const FieldElementT& column0_row14 = neighbors[kColumn0Row14Neighbor];
  const FieldElementT& column0_row15 = neighbors[kColumn0Row15Neighbor];
  const FieldElementT& column1_row0 = neighbors[kColumn1Row0Neighbor];
  const FieldElementT& column1_row1 = neighbors[kColumn1Row1Neighbor];
  const FieldElementT& column1_row2 = neighbors[kColumn1Row2Neighbor];
  const FieldElementT& column1_row4 = neighbors[kColumn1Row4Neighbor];
  const FieldElementT& column1_row6 = neighbors[kColumn1Row6Neighbor];
  const FieldElementT& column1_row8 = neighbors[kColumn1Row8Neighbor];
  const FieldElementT& column1_row10 = neighbors[kColumn1Row10Neighbor];
  const FieldElementT& column1_row12 = neighbors[kColumn1Row12Neighbor];
  const FieldElementT& column1_row14 = neighbors[kColumn1Row14Neighbor];
  const FieldElementT& column1_row16 = neighbors[kColumn1Row16Neighbor];
  const FieldElementT& column1_row18 = neighbors[kColumn1Row18Neighbor];
  const FieldElementT& column1_row20 = neighbors[kColumn1Row20Neighbor];
  const FieldElementT& column1_row22 = neighbors[kColumn1Row22Neighbor];
  const FieldElementT& column1_row24 = neighbors[kColumn1Row24Neighbor];
  const FieldElementT& column1_row26 = neighbors[kColumn1Row26Neighbor];
  const FieldElementT& column1_row28 = neighbors[kColumn1Row28Neighbor];
  const FieldElementT& column1_row30 = neighbors[kColumn1Row30Neighbor];
  const FieldElementT& column1_row32 = neighbors[kColumn1Row32Neighbor];
  const FieldElementT& column1_row33 = neighbors[kColumn1Row33Neighbor];
  const FieldElementT& column1_row64 = neighbors[kColumn1Row64Neighbor];
  const FieldElementT& column1_row65 = neighbors[kColumn1Row65Neighbor];
  const FieldElementT& column1_row88 = neighbors[kColumn1Row88Neighbor];
  const FieldElementT& column1_row90 = neighbors[kColumn1Row90Neighbor];
  const FieldElementT& column1_row92 = neighbors[kColumn1Row92Neighbor];
  const FieldElementT& column1_row94 = neighbors[kColumn1Row94Neighbor];
  const FieldElementT& column1_row96 = neighbors[kColumn1Row96Neighbor];
  const FieldElementT& column1_row97 = neighbors[kColumn1Row97Neighbor];
  const FieldElementT& column1_row120 = neighbors[kColumn1Row120Neighbor];
  const FieldElementT& column1_row122 = neighbors[kColumn1Row122Neighbor];
  const FieldElementT& column1_row124 = neighbors[kColumn1Row124Neighbor];
  const FieldElementT& column1_row126 = neighbors[kColumn1Row126Neighbor];
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
  const FieldElementT& column3_row10 = neighbors[kColumn3Row10Neighbor];
  const FieldElementT& column3_row11 = neighbors[kColumn3Row11Neighbor];
  const FieldElementT& column3_row12 = neighbors[kColumn3Row12Neighbor];
  const FieldElementT& column3_row13 = neighbors[kColumn3Row13Neighbor];
  const FieldElementT& column3_row16 = neighbors[kColumn3Row16Neighbor];
  const FieldElementT& column3_row26 = neighbors[kColumn3Row26Neighbor];
  const FieldElementT& column3_row27 = neighbors[kColumn3Row27Neighbor];
  const FieldElementT& column3_row42 = neighbors[kColumn3Row42Neighbor];
  const FieldElementT& column3_row43 = neighbors[kColumn3Row43Neighbor];
  const FieldElementT& column3_row58 = neighbors[kColumn3Row58Neighbor];
  const FieldElementT& column3_row74 = neighbors[kColumn3Row74Neighbor];
  const FieldElementT& column3_row75 = neighbors[kColumn3Row75Neighbor];
  const FieldElementT& column3_row91 = neighbors[kColumn3Row91Neighbor];
  const FieldElementT& column3_row122 = neighbors[kColumn3Row122Neighbor];
  const FieldElementT& column3_row123 = neighbors[kColumn3Row123Neighbor];
  const FieldElementT& column3_row154 = neighbors[kColumn3Row154Neighbor];
  const FieldElementT& column3_row202 = neighbors[kColumn3Row202Neighbor];
  const FieldElementT& column3_row522 = neighbors[kColumn3Row522Neighbor];
  const FieldElementT& column3_row523 = neighbors[kColumn3Row523Neighbor];
  const FieldElementT& column3_row1034 = neighbors[kColumn3Row1034Neighbor];
  const FieldElementT& column3_row1035 = neighbors[kColumn3Row1035Neighbor];
  const FieldElementT& column3_row2058 = neighbors[kColumn3Row2058Neighbor];
  const FieldElementT& column4_row0 = neighbors[kColumn4Row0Neighbor];
  const FieldElementT& column4_row1 = neighbors[kColumn4Row1Neighbor];
  const FieldElementT& column4_row2 = neighbors[kColumn4Row2Neighbor];
  const FieldElementT& column4_row3 = neighbors[kColumn4Row3Neighbor];
  const FieldElementT& column5_row0 = neighbors[kColumn5Row0Neighbor];
  const FieldElementT& column5_row1 = neighbors[kColumn5Row1Neighbor];
  const FieldElementT& column5_row2 = neighbors[kColumn5Row2Neighbor];
  const FieldElementT& column5_row3 = neighbors[kColumn5Row3Neighbor];
  const FieldElementT& column5_row4 = neighbors[kColumn5Row4Neighbor];
  const FieldElementT& column5_row5 = neighbors[kColumn5Row5Neighbor];
  const FieldElementT& column5_row6 = neighbors[kColumn5Row6Neighbor];
  const FieldElementT& column5_row7 = neighbors[kColumn5Row7Neighbor];
  const FieldElementT& column5_row8 = neighbors[kColumn5Row8Neighbor];
  const FieldElementT& column5_row12 = neighbors[kColumn5Row12Neighbor];
  const FieldElementT& column5_row28 = neighbors[kColumn5Row28Neighbor];
  const FieldElementT& column5_row44 = neighbors[kColumn5Row44Neighbor];
  const FieldElementT& column5_row60 = neighbors[kColumn5Row60Neighbor];
  const FieldElementT& column5_row76 = neighbors[kColumn5Row76Neighbor];
  const FieldElementT& column5_row92 = neighbors[kColumn5Row92Neighbor];
  const FieldElementT& column5_row108 = neighbors[kColumn5Row108Neighbor];
  const FieldElementT& column5_row124 = neighbors[kColumn5Row124Neighbor];
  const FieldElementT& column5_row1021 = neighbors[kColumn5Row1021Neighbor];
  const FieldElementT& column5_row1023 = neighbors[kColumn5Row1023Neighbor];
  const FieldElementT& column5_row1025 = neighbors[kColumn5Row1025Neighbor];
  const FieldElementT& column5_row1027 = neighbors[kColumn5Row1027Neighbor];
  const FieldElementT& column5_row2045 = neighbors[kColumn5Row2045Neighbor];
  const FieldElementT& column6_row0 = neighbors[kColumn6Row0Neighbor];
  const FieldElementT& column6_row1 = neighbors[kColumn6Row1Neighbor];
  const FieldElementT& column6_row2 = neighbors[kColumn6Row2Neighbor];
  const FieldElementT& column6_row3 = neighbors[kColumn6Row3Neighbor];
  const FieldElementT& column6_row4 = neighbors[kColumn6Row4Neighbor];
  const FieldElementT& column6_row5 = neighbors[kColumn6Row5Neighbor];
  const FieldElementT& column6_row7 = neighbors[kColumn6Row7Neighbor];
  const FieldElementT& column6_row9 = neighbors[kColumn6Row9Neighbor];
  const FieldElementT& column6_row11 = neighbors[kColumn6Row11Neighbor];
  const FieldElementT& column6_row13 = neighbors[kColumn6Row13Neighbor];
  const FieldElementT& column6_row17 = neighbors[kColumn6Row17Neighbor];
  const FieldElementT& column6_row25 = neighbors[kColumn6Row25Neighbor];
  const FieldElementT& column6_row768 = neighbors[kColumn6Row768Neighbor];
  const FieldElementT& column6_row772 = neighbors[kColumn6Row772Neighbor];
  const FieldElementT& column6_row784 = neighbors[kColumn6Row784Neighbor];
  const FieldElementT& column6_row788 = neighbors[kColumn6Row788Neighbor];
  const FieldElementT& column6_row1004 = neighbors[kColumn6Row1004Neighbor];
  const FieldElementT& column6_row1008 = neighbors[kColumn6Row1008Neighbor];
  const FieldElementT& column6_row1022 = neighbors[kColumn6Row1022Neighbor];
  const FieldElementT& column6_row1024 = neighbors[kColumn6Row1024Neighbor];
  const FieldElementT& column7_inter1_row0 = neighbors[kColumn7Inter1Row0Neighbor];
  const FieldElementT& column7_inter1_row1 = neighbors[kColumn7Inter1Row1Neighbor];
  const FieldElementT& column8_inter1_row0 = neighbors[kColumn8Inter1Row0Neighbor];
  const FieldElementT& column8_inter1_row1 = neighbors[kColumn8Inter1Row1Neighbor];
  const FieldElementT& column9_inter1_row0 = neighbors[kColumn9Inter1Row0Neighbor];
  const FieldElementT& column9_inter1_row1 = neighbors[kColumn9Inter1Row1Neighbor];
  const FieldElementT& column9_inter1_row2 = neighbors[kColumn9Inter1Row2Neighbor];
  const FieldElementT& column9_inter1_row5 = neighbors[kColumn9Inter1Row5Neighbor];

  ASSERT_VERIFIER(periodic_columns.size() == 2, "periodic_columns should contain 2 elements.");
  const FieldElementT& pedersen__points__x = periodic_columns[kPedersenPointsXPeriodicColumn];
  const FieldElementT& pedersen__points__y = periodic_columns[kPedersenPointsYPeriodicColumn];

  const FieldElementT cpu__decode__opcode_rc__bit_0 =
      (column0_row0) - ((column0_row1) + (column0_row1));
  const FieldElementT cpu__decode__opcode_rc__bit_2 =
      (column0_row2) - ((column0_row3) + (column0_row3));
  const FieldElementT cpu__decode__opcode_rc__bit_4 =
      (column0_row4) - ((column0_row5) + (column0_row5));
  const FieldElementT cpu__decode__opcode_rc__bit_3 =
      (column0_row3) - ((column0_row4) + (column0_row4));
  const FieldElementT cpu__decode__flag_op1_base_op0_0 =
      (FieldElementT::One()) -
      (((cpu__decode__opcode_rc__bit_2) + (cpu__decode__opcode_rc__bit_4)) +
       (cpu__decode__opcode_rc__bit_3));
  const FieldElementT cpu__decode__opcode_rc__bit_5 =
      (column0_row5) - ((column0_row6) + (column0_row6));
  const FieldElementT cpu__decode__opcode_rc__bit_6 =
      (column0_row6) - ((column0_row7) + (column0_row7));
  const FieldElementT cpu__decode__opcode_rc__bit_9 =
      (column0_row9) - ((column0_row10) + (column0_row10));
  const FieldElementT cpu__decode__flag_res_op1_0 =
      (FieldElementT::One()) -
      (((cpu__decode__opcode_rc__bit_5) + (cpu__decode__opcode_rc__bit_6)) +
       (cpu__decode__opcode_rc__bit_9));
  const FieldElementT cpu__decode__opcode_rc__bit_7 =
      (column0_row7) - ((column0_row8) + (column0_row8));
  const FieldElementT cpu__decode__opcode_rc__bit_8 =
      (column0_row8) - ((column0_row9) + (column0_row9));
  const FieldElementT cpu__decode__flag_pc_update_regular_0 =
      (FieldElementT::One()) -
      (((cpu__decode__opcode_rc__bit_7) + (cpu__decode__opcode_rc__bit_8)) +
       (cpu__decode__opcode_rc__bit_9));
  const FieldElementT cpu__decode__opcode_rc__bit_12 =
      (column0_row12) - ((column0_row13) + (column0_row13));
  const FieldElementT cpu__decode__opcode_rc__bit_13 =
      (column0_row13) - ((column0_row14) + (column0_row14));
  const FieldElementT cpu__decode__fp_update_regular_0 =
      (FieldElementT::One()) -
      ((cpu__decode__opcode_rc__bit_12) + (cpu__decode__opcode_rc__bit_13));
  const FieldElementT cpu__decode__opcode_rc__bit_1 =
      (column0_row1) - ((column0_row2) + (column0_row2));
  const FieldElementT npc_reg_0 =
      ((column3_row0) + (cpu__decode__opcode_rc__bit_2)) + (FieldElementT::One());
  const FieldElementT cpu__decode__opcode_rc__bit_10 =
      (column0_row10) - ((column0_row11) + (column0_row11));
  const FieldElementT cpu__decode__opcode_rc__bit_11 =
      (column0_row11) - ((column0_row12) + (column0_row12));
  const FieldElementT cpu__decode__opcode_rc__bit_14 =
      (column0_row14) - ((column0_row15) + (column0_row15));
  const FieldElementT memory__address_diff_0 = (column4_row2) - (column4_row0);
  const FieldElementT rc16__diff_0 = (column5_row6) - (column5_row2);
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_0 =
      (column6_row0) - ((column6_row4) + (column6_row4));
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash0__ec_subset_sum__bit_0);
  const FieldElementT rc_builtin__value0_0 = column5_row12;
  const FieldElementT rc_builtin__value1_0 =
      ((rc_builtin__value0_0) * (offset_size_)) + (column5_row28);
  const FieldElementT rc_builtin__value2_0 =
      ((rc_builtin__value1_0) * (offset_size_)) + (column5_row44);
  const FieldElementT rc_builtin__value3_0 =
      ((rc_builtin__value2_0) * (offset_size_)) + (column5_row60);
  const FieldElementT rc_builtin__value4_0 =
      ((rc_builtin__value3_0) * (offset_size_)) + (column5_row76);
  const FieldElementT rc_builtin__value5_0 =
      ((rc_builtin__value4_0) * (offset_size_)) + (column5_row92);
  const FieldElementT rc_builtin__value6_0 =
      ((rc_builtin__value5_0) * (offset_size_)) + (column5_row108);
  const FieldElementT rc_builtin__value7_0 =
      ((rc_builtin__value6_0) * (offset_size_)) + (column5_row124);
  const FieldElementT bitwise__sum_var_0_0 =
      (((((((column1_row0) + ((column1_row2) * (FieldElementT::ConstexprFromBigInt(0x2_Z)))) +
           ((column1_row4) * (FieldElementT::ConstexprFromBigInt(0x4_Z)))) +
          ((column1_row6) * (FieldElementT::ConstexprFromBigInt(0x8_Z)))) +
         ((column1_row8) * (FieldElementT::ConstexprFromBigInt(0x10000000000000000_Z)))) +
        ((column1_row10) * (FieldElementT::ConstexprFromBigInt(0x20000000000000000_Z)))) +
       ((column1_row12) * (FieldElementT::ConstexprFromBigInt(0x40000000000000000_Z)))) +
      ((column1_row14) * (FieldElementT::ConstexprFromBigInt(0x80000000000000000_Z)));
  const FieldElementT bitwise__sum_var_8_0 =
      ((((((((column1_row16) *
             (FieldElementT::ConstexprFromBigInt(0x100000000000000000000000000000000_Z))) +
            ((column1_row18) *
             (FieldElementT::ConstexprFromBigInt(0x200000000000000000000000000000000_Z)))) +
           ((column1_row20) *
            (FieldElementT::ConstexprFromBigInt(0x400000000000000000000000000000000_Z)))) +
          ((column1_row22) *
           (FieldElementT::ConstexprFromBigInt(0x800000000000000000000000000000000_Z)))) +
         ((column1_row24) * (FieldElementT::ConstexprFromBigInt(
                                0x1000000000000000000000000000000000000000000000000_Z)))) +
        ((column1_row26) * (FieldElementT::ConstexprFromBigInt(
                               0x2000000000000000000000000000000000000000000000000_Z)))) +
       ((column1_row28) * (FieldElementT::ConstexprFromBigInt(
                              0x4000000000000000000000000000000000000000000000000_Z)))) +
      ((column1_row30) *
       (FieldElementT::ConstexprFromBigInt(0x8000000000000000000000000000000000000000000000000_Z)));
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
      // Compute a sum of constraints with numerator = domain14.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for diluted_check/permutation/step0:
        const FieldElementT constraint =
            (((diluted_check__permutation__interaction_elm_) - (column2_row1)) *
             (column8_inter1_row1)) -
            (((diluted_check__permutation__interaction_elm_) - (column1_row1)) *
             (column8_inter1_row0));
        inner_sum += random_coefficients[48] * constraint;
      }
      {
        // Constraint expression for diluted_check/step:
        const FieldElementT constraint =
            (column7_inter1_row1) -
            (((column7_inter1_row0) *
              ((FieldElementT::One()) +
               ((diluted_check__interaction_z_) * ((column2_row1) - (column2_row0))))) +
             (((diluted_check__interaction_alpha_) * ((column2_row1) - (column2_row0))) *
              ((column2_row1) - (column2_row0))));
        inner_sum += random_coefficients[52] * constraint;
      }
      outer_sum += inner_sum * domain14;
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
        const FieldElementT constraint = column0_row0;
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
            (((((((column0_row0) * (offset_size_)) + (column5_row4)) * (offset_size_)) +
               (column5_row8)) *
              (offset_size_)) +
             (column5_row0));
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
            ((((cpu__decode__opcode_rc__bit_0) * (column6_row9)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_0)) * (column6_row1))) +
             (column5_row0));
        inner_sum += random_coefficients[7] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem0_addr:
        const FieldElementT constraint =
            ((column3_row4) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_1) * (column6_row9)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_1)) * (column6_row1))) +
             (column5_row8));
        inner_sum += random_coefficients[8] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem1_addr:
        const FieldElementT constraint = ((column3_row12) + (half_offset_size_)) -
                                         ((((((cpu__decode__opcode_rc__bit_2) * (column3_row0)) +
                                             ((cpu__decode__opcode_rc__bit_4) * (column6_row1))) +
                                            ((cpu__decode__opcode_rc__bit_3) * (column6_row9))) +
                                           ((cpu__decode__flag_op1_base_op0_0) * (column3_row5))) +
                                          (column5_row4));
        inner_sum += random_coefficients[9] * constraint;
      }
      {
        // Constraint expression for cpu/operands/ops_mul:
        const FieldElementT constraint = (column6_row5) - ((column3_row5) * (column3_row13));
        inner_sum += random_coefficients[10] * constraint;
      }
      {
        // Constraint expression for cpu/operands/res:
        const FieldElementT constraint =
            (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column6_row13)) -
            ((((cpu__decode__opcode_rc__bit_5) * ((column3_row5) + (column3_row13))) +
              ((cpu__decode__opcode_rc__bit_6) * (column6_row5))) +
             ((cpu__decode__flag_res_op1_0) * (column3_row13)));
        inner_sum += random_coefficients[11] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_fp:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column3_row9) - (column6_row9));
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
            (cpu__decode__opcode_rc__bit_12) * ((column5_row0) - (half_offset_size_));
        inner_sum += random_coefficients[20] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off1:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) *
            ((column5_row8) - ((half_offset_size_) + (FieldElementT::One())));
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
            (((column5_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z))) - (half_offset_size_));
        inner_sum += random_coefficients[23] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/off2:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_13) *
            (((column5_row4) + (FieldElementT::One())) - (half_offset_size_));
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
            (cpu__decode__opcode_rc__bit_14) * ((column3_row9) - (column6_row13));
        inner_sum += random_coefficients[26] * constraint;
      }
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

    {
      // Compute a sum of constraints with numerator = domain15.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp0:
        const FieldElementT constraint =
            (column6_row3) - ((cpu__decode__opcode_rc__bit_9) * (column3_row9));
        inner_sum += random_coefficients[12] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp1:
        const FieldElementT constraint = (column6_row11) - ((column6_row3) * (column6_row13));
        inner_sum += random_coefficients[13] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_negative:
        const FieldElementT constraint =
            ((((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column3_row16)) +
             ((column6_row3) * ((column3_row16) - ((column3_row0) + (column3_row13))))) -
            ((((cpu__decode__flag_pc_update_regular_0) * (npc_reg_0)) +
              ((cpu__decode__opcode_rc__bit_7) * (column6_row13))) +
             ((cpu__decode__opcode_rc__bit_8) * ((column3_row0) + (column6_row13))));
        inner_sum += random_coefficients[14] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_positive:
        const FieldElementT constraint =
            ((column6_row11) - (cpu__decode__opcode_rc__bit_9)) * ((column3_row16) - (npc_reg_0));
        inner_sum += random_coefficients[15] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_ap/ap_update:
        const FieldElementT constraint =
            (column6_row17) -
            ((((column6_row1) + ((cpu__decode__opcode_rc__bit_10) * (column6_row13))) +
              (cpu__decode__opcode_rc__bit_11)) +
             ((cpu__decode__opcode_rc__bit_12) * (FieldElementT::ConstexprFromBigInt(0x2_Z))));
        inner_sum += random_coefficients[16] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_fp/fp_update:
        const FieldElementT constraint =
            (column6_row25) - ((((cpu__decode__fp_update_regular_0) * (column6_row9)) +
                                ((cpu__decode__opcode_rc__bit_13) * (column3_row9))) +
                               ((cpu__decode__opcode_rc__bit_12) *
                                ((column6_row1) + (FieldElementT::ConstexprFromBigInt(0x2_Z)))));
        inner_sum += random_coefficients[17] * constraint;
      }
      outer_sum += inner_sum * domain15;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain4);
  }

  {
    // Compute a sum of constraints with denominator = domain16.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for initial_ap:
        const FieldElementT constraint = (column6_row1) - (initial_ap_);
        inner_sum += random_coefficients[27] * constraint;
      }
      {
        // Constraint expression for initial_fp:
        const FieldElementT constraint = (column6_row9) - (initial_ap_);
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
               (column9_inter1_row0)) +
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
            ((((rc16__perm__interaction_elm_) - (column5_row2)) * (column9_inter1_row1)) +
             (column5_row0)) -
            (rc16__perm__interaction_elm_);
        inner_sum += random_coefficients[41] * constraint;
      }
      {
        // Constraint expression for rc16/minimum:
        const FieldElementT constraint = (column5_row2) - (rc_min_);
        inner_sum += random_coefficients[45] * constraint;
      }
      {
        // Constraint expression for diluted_check/permutation/init0:
        const FieldElementT constraint =
            ((((diluted_check__permutation__interaction_elm_) - (column2_row0)) *
              (column8_inter1_row0)) +
             (column1_row0)) -
            (diluted_check__permutation__interaction_elm_);
        inner_sum += random_coefficients[47] * constraint;
      }
      {
        // Constraint expression for diluted_check/init:
        const FieldElementT constraint = (column7_inter1_row0) - (FieldElementT::One());
        inner_sum += random_coefficients[50] * constraint;
      }
      {
        // Constraint expression for diluted_check/first_element:
        const FieldElementT constraint = (column2_row0) - (diluted_check__first_elm_);
        inner_sum += random_coefficients[51] * constraint;
      }
      {
        // Constraint expression for pedersen/init_addr:
        const FieldElementT constraint = (column3_row10) - (initial_pedersen_addr_);
        inner_sum += random_coefficients[74] * constraint;
      }
      {
        // Constraint expression for rc_builtin/init_addr:
        const FieldElementT constraint = (column3_row74) - (initial_rc_addr_);
        inner_sum += random_coefficients[81] * constraint;
      }
      {
        // Constraint expression for bitwise/init_var_pool_addr:
        const FieldElementT constraint = (column3_row26) - (initial_bitwise_addr_);
        inner_sum += random_coefficients[82] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain16);
  }

  {
    // Compute a sum of constraints with denominator = domain15.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for final_ap:
        const FieldElementT constraint = (column6_row1) - (final_ap_);
        inner_sum += random_coefficients[30] * constraint;
      }
      {
        // Constraint expression for final_fp:
        const FieldElementT constraint = (column6_row9) - (initial_ap_);
        inner_sum += random_coefficients[31] * constraint;
      }
      {
        // Constraint expression for final_pc:
        const FieldElementT constraint = (column3_row0) - (final_pc_);
        inner_sum += random_coefficients[32] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain15);
  }

  {
    // Compute a sum of constraints with denominator = domain1.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain17.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/step0:
        const FieldElementT constraint =
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column4_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column4_row3)))) *
             (column9_inter1_row2)) -
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column3_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column3_row3)))) *
             (column9_inter1_row0));
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
      outer_sum += inner_sum * domain17;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain1);
  }

  {
    // Compute a sum of constraints with denominator = domain17.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/last:
        const FieldElementT constraint =
            (column9_inter1_row0) - (memory__multi_column_perm__perm__public_memory_prod_);
        inner_sum += random_coefficients[35] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain17);
  }

  {
    // Compute a sum of constraints with denominator = domain2.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain18.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc16/perm/step0:
        const FieldElementT constraint =
            (((rc16__perm__interaction_elm_) - (column5_row6)) * (column9_inter1_row5)) -
            (((rc16__perm__interaction_elm_) - (column5_row4)) * (column9_inter1_row1));
        inner_sum += random_coefficients[42] * constraint;
      }
      {
        // Constraint expression for rc16/diff_is_bit:
        const FieldElementT constraint = ((rc16__diff_0) * (rc16__diff_0)) - (rc16__diff_0);
        inner_sum += random_coefficients[44] * constraint;
      }
      outer_sum += inner_sum * domain18;
    }

    {
      // Compute a sum of constraints with numerator = domain9.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/booleanity_test:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_0) *
            ((pedersen__hash0__ec_subset_sum__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[60] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/slope:
        const FieldElementT constraint =
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column5_row3) - (pedersen__points__y))) -
            ((column6_row2) * ((column5_row1) - (pedersen__points__x)));
        inner_sum += random_coefficients[63] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column6_row2) * (column6_row2)) -
            ((pedersen__hash0__ec_subset_sum__bit_0) *
             (((column5_row1) + (pedersen__points__x)) + (column5_row5)));
        inner_sum += random_coefficients[64] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column5_row3) + (column5_row7))) -
            ((column6_row2) * ((column5_row1) - (column5_row5)));
        inner_sum += random_coefficients[65] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column5_row5) - (column5_row1));
        inner_sum += random_coefficients[66] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column5_row7) - (column5_row3));
        inner_sum += random_coefficients[67] * constraint;
      }
      outer_sum += inner_sum * domain9;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain2);
  }

  {
    // Compute a sum of constraints with denominator = domain18.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc16/perm/last:
        const FieldElementT constraint = (column9_inter1_row1) - (rc16__perm__public_memory_prod_);
        inner_sum += random_coefficients[43] * constraint;
      }
      {
        // Constraint expression for rc16/maximum:
        const FieldElementT constraint = (column5_row2) - (rc_max_);
        inner_sum += random_coefficients[46] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain18);
  }

  {
    // Compute a sum of constraints with denominator = domain14.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for diluted_check/permutation/last:
        const FieldElementT constraint =
            (column8_inter1_row0) - (diluted_check__permutation__public_memory_prod_);
        inner_sum += random_coefficients[49] * constraint;
      }
      {
        // Constraint expression for diluted_check/last:
        const FieldElementT constraint = (column7_inter1_row0) - (diluted_check__final_cum_val_);
        inner_sum += random_coefficients[53] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain14);
  }

  {
    // Compute a sum of constraints with denominator = domain10.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column6_row7) * ((column6_row0) - ((column6_row4) + (column6_row4)));
        inner_sum += random_coefficients[54] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column6_row7) *
            ((column6_row4) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column6_row768)));
        inner_sum += random_coefficients[55] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column6_row7) -
            ((column6_row1022) * ((column6_row768) - ((column6_row772) + (column6_row772))));
        inner_sum += random_coefficients[56] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column6_row1022) *
            ((column6_row772) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column6_row784)));
        inner_sum += random_coefficients[57] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column6_row1022) - (((column6_row1004) - ((column6_row1008) + (column6_row1008))) *
                                 ((column6_row784) - ((column6_row788) + (column6_row788))));
        inner_sum += random_coefficients[58] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column6_row1004) - ((column6_row1008) + (column6_row1008))) *
            ((column6_row788) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column6_row1004)));
        inner_sum += random_coefficients[59] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain12.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/copy_point/x:
        const FieldElementT constraint = (column5_row1025) - (column5_row1021);
        inner_sum += random_coefficients[68] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/copy_point/y:
        const FieldElementT constraint = (column5_row1027) - (column5_row1023);
        inner_sum += random_coefficients[69] * constraint;
      }
      outer_sum += inner_sum * domain12;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain10);
  }

  {
    // Compute a sum of constraints with denominator = domain11.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column6_row0;
        inner_sum += random_coefficients[61] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain11);
  }

  {
    // Compute a sum of constraints with denominator = domain9.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column6_row0;
        inner_sum += random_coefficients[62] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain9);
  }

  {
    // Compute a sum of constraints with denominator = domain13.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/init/x:
        const FieldElementT constraint = (column5_row1) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[70] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/init/y:
        const FieldElementT constraint = (column5_row3) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[71] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value0:
        const FieldElementT constraint = (column3_row11) - (column6_row0);
        inner_sum += random_coefficients[72] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value0:
        const FieldElementT constraint = (column3_row1035) - (column6_row1024);
        inner_sum += random_coefficients[75] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_addr:
        const FieldElementT constraint =
            (column3_row1034) - ((column3_row10) + (FieldElementT::One()));
        inner_sum += random_coefficients[76] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value0:
        const FieldElementT constraint = (column3_row523) - (column5_row2045);
        inner_sum += random_coefficients[77] * constraint;
      }
      {
        // Constraint expression for pedersen/output_addr:
        const FieldElementT constraint =
            (column3_row522) - ((column3_row1034) + (FieldElementT::One()));
        inner_sum += random_coefficients[78] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain19.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input0_addr:
        const FieldElementT constraint =
            (column3_row2058) - ((column3_row522) + (FieldElementT::One()));
        inner_sum += random_coefficients[73] * constraint;
      }
      outer_sum += inner_sum * domain19;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain13);
  }

  {
    // Compute a sum of constraints with denominator = domain6.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc_builtin/value:
        const FieldElementT constraint = (rc_builtin__value7_0) - (column3_row75);
        inner_sum += random_coefficients[79] * constraint;
      }
      {
        // Constraint expression for bitwise/x_or_y_addr:
        const FieldElementT constraint =
            (column3_row42) - ((column3_row122) + (FieldElementT::One()));
        inner_sum += random_coefficients[84] * constraint;
      }
      {
        // Constraint expression for bitwise/or_is_and_plus_xor:
        const FieldElementT constraint = (column3_row43) - ((column3_row91) + (column3_row123));
        inner_sum += random_coefficients[87] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking192:
        const FieldElementT constraint =
            (((column1_row88) + (column1_row120)) * (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
            (column1_row1);
        inner_sum += random_coefficients[89] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking193:
        const FieldElementT constraint =
            (((column1_row90) + (column1_row122)) * (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
            (column1_row65);
        inner_sum += random_coefficients[90] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking194:
        const FieldElementT constraint =
            (((column1_row92) + (column1_row124)) * (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
            (column1_row33);
        inner_sum += random_coefficients[91] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking195:
        const FieldElementT constraint =
            (((column1_row94) + (column1_row126)) * (FieldElementT::ConstexprFromBigInt(0x100_Z))) -
            (column1_row97);
        inner_sum += random_coefficients[92] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain20.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc_builtin/addr_step:
        const FieldElementT constraint =
            (column3_row202) - ((column3_row74) + (FieldElementT::One()));
        inner_sum += random_coefficients[80] * constraint;
      }
      {
        // Constraint expression for bitwise/next_var_pool_addr:
        const FieldElementT constraint =
            (column3_row154) - ((column3_row42) + (FieldElementT::One()));
        inner_sum += random_coefficients[85] * constraint;
      }
      outer_sum += inner_sum * domain20;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain6);
  }

  {
    // Compute a sum of constraints with denominator = domain5.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain7.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/step_var_pool_addr:
        const FieldElementT constraint =
            (column3_row58) - ((column3_row26) + (FieldElementT::One()));
        inner_sum += random_coefficients[83] * constraint;
      }
      outer_sum += inner_sum * domain7;
    }

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/partition:
        const FieldElementT constraint =
            ((bitwise__sum_var_0_0) + (bitwise__sum_var_8_0)) - (column3_row27);
        inner_sum += random_coefficients[86] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain5);
  }

  {
    // Compute a sum of constraints with denominator = domain8.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/addition_is_xor_with_and:
        const FieldElementT constraint = ((column1_row0) + (column1_row32)) -
                                         (((column1_row96) + (column1_row64)) + (column1_row64));
        inner_sum += random_coefficients[88] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain8);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> CpuAirDefinition<FieldElementT, 4>::DomainEvalsAtPoint(
    gsl::span<const FieldElementT> point_powers,
    gsl::span<const FieldElementT> shifts) const {
  const FieldElementT& domain0 = (point_powers[1]) - (FieldElementT::One());
  const FieldElementT& domain1 = (point_powers[2]) - (FieldElementT::One());
  const FieldElementT& domain2 = (point_powers[3]) - (FieldElementT::One());
  const FieldElementT& domain3 = (point_powers[4]) - (shifts[0]);
  const FieldElementT& domain4 = (point_powers[4]) - (FieldElementT::One());
  const FieldElementT& domain5 = (point_powers[5]) - (FieldElementT::One());
  const FieldElementT& domain6 = (point_powers[6]) - (FieldElementT::One());
  const FieldElementT& domain7 = (point_powers[6]) - (shifts[1]);
  const FieldElementT& domain8 =
      ((point_powers[6]) - (shifts[2])) * ((point_powers[6]) - (shifts[3])) *
      ((point_powers[6]) - (shifts[4])) * ((point_powers[6]) - (shifts[5])) *
      ((point_powers[6]) - (shifts[6])) * ((point_powers[6]) - (shifts[7])) *
      ((point_powers[6]) - (shifts[8])) * ((point_powers[6]) - (shifts[9])) *
      ((point_powers[6]) - (shifts[10])) * ((point_powers[6]) - (shifts[11])) *
      ((point_powers[6]) - (shifts[12])) * ((point_powers[6]) - (shifts[13])) *
      ((point_powers[6]) - (shifts[14])) * ((point_powers[6]) - (shifts[15])) *
      ((point_powers[6]) - (shifts[16])) * (domain6);
  const FieldElementT& domain9 = (point_powers[7]) - (shifts[17]);
  const FieldElementT& domain10 = (point_powers[7]) - (FieldElementT::One());
  const FieldElementT& domain11 = (point_powers[7]) - (shifts[18]);
  const FieldElementT& domain12 = (point_powers[8]) - (shifts[19]);
  const FieldElementT& domain13 = (point_powers[8]) - (FieldElementT::One());
  return {
      domain0, domain1, domain2, domain3,  domain4,  domain5,  domain6,
      domain7, domain8, domain9, domain10, domain11, domain12, domain13,
  };
}

template <typename FieldElementT>
TraceGenerationContext CpuAirDefinition<FieldElementT, 4>::GetTraceGenerationContext() const {
  TraceGenerationContext ctx;

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 128)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 128)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 128)) - (1)) <= (SafeDiv(trace_length_, 128)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 128)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 128)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 128)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 128)) <= (SafeDiv(trace_length_, 128)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 128)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 128)), "Index out of range.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 2048)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 2048)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 2048)) <= (SafeDiv(trace_length_, 2048)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 2048)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 2048)), "Index out of range.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 2048)), "Index out of range.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 2048)) - (1)) <= (SafeDiv(trace_length_, 2048)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 2048)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 2048)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE(((trace_length_) + (-1)) < (trace_length_), "Index out of range.");

  ASSERT_RELEASE(((trace_length_) + (-1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((1) <= (trace_length_), "step must not exceed dimension.");

  ASSERT_RELEASE(((trace_length_) - (1)) <= (trace_length_), "Index out of range.");

  ASSERT_RELEASE(((trace_length_) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (trace_length_), "Index out of range.");

  ASSERT_RELEASE((0) <= ((trace_length_) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE((trace_length_) <= (trace_length_), "Index out of range.");

  ASSERT_RELEASE((trace_length_) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (trace_length_), "Index out of range.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 4)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 4)) + (-1)) < (SafeDiv(trace_length_, 4)), "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 4)) + (-1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 4)), "Index out of range.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 4)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 4)) - (1)) <= (SafeDiv(trace_length_, 4)), "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 4)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 4)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 4)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 4)) <= (SafeDiv(trace_length_, 4)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 4)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 16)), "Dimension should be a power of 2.");

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
      "cpu/decode/opcode_rc/column",
      VirtualColumn(/*column=*/kColumn0Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "diluted_pool", VirtualColumn(/*column=*/kColumn1Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "diluted_check/permuted_values",
      VirtualColumn(/*column=*/kColumn2Column, /*step=*/1, /*row_offset=*/0));
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
      "rc16_pool", VirtualColumn(/*column=*/kColumn5Column, /*step=*/4, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "rc16/sorted", VirtualColumn(/*column=*/kColumn5Column, /*step=*/4, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/4, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/4, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/4, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/4, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "cpu/registers/ap", VirtualColumn(/*column=*/kColumn6Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/registers/fp", VirtualColumn(/*column=*/kColumn6Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "cpu/operands/ops_mul",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/16, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "cpu/operands/res", VirtualColumn(/*column=*/kColumn6Column, /*step=*/16, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp0",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/16, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp1",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/16, /*row_offset=*/11));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/1024, /*row_offset=*/1022));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/1024, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "diluted_check/cumulative_value",
      VirtualColumn(
          /*column=*/kColumn7Inter1Column - kNumColumnsFirst, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "diluted_check/permutation/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn8Inter1Column - kNumColumnsFirst, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/multi_column_perm/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn9Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "rc16/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn9Inter1Column - kNumColumnsFirst, /*step=*/4, /*row_offset=*/1));
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
      "cpu/decode/off0", VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/off1", VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/decode/off2", VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/4));
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
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "orig/public_memory/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "pedersen/input0/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/2048, /*row_offset=*/10));
  ctx.AddVirtualColumn(
      "pedersen/input0/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/2048, /*row_offset=*/11));
  ctx.AddVirtualColumn(
      "pedersen/input1/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/2048, /*row_offset=*/1034));
  ctx.AddVirtualColumn(
      "pedersen/input1/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/2048, /*row_offset=*/1035));
  ctx.AddVirtualColumn(
      "pedersen/output/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/2048, /*row_offset=*/522));
  ctx.AddVirtualColumn(
      "pedersen/output/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/2048, /*row_offset=*/523));
  ctx.AddVirtualColumn(
      "rc_builtin/mem/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/74));
  ctx.AddVirtualColumn(
      "rc_builtin/mem/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/75));
  ctx.AddVirtualColumn(
      "rc_builtin/inner_rc",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "bitwise/x/addr", VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/26));
  ctx.AddVirtualColumn(
      "bitwise/x/value", VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/27));
  ctx.AddVirtualColumn(
      "bitwise/y/addr", VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/58));
  ctx.AddVirtualColumn(
      "bitwise/y/value", VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/59));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/90));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/91));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/122));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/123));
  ctx.AddVirtualColumn(
      "bitwise/x_or_y/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/42));
  ctx.AddVirtualColumn(
      "bitwise/x_or_y/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/43));
  ctx.AddVirtualColumn(
      "bitwise/diluted_var_pool",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "bitwise/x", VirtualColumn(/*column=*/kColumn1Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "bitwise/y", VirtualColumn(/*column=*/kColumn1Column, /*step=*/2, /*row_offset=*/32));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y", VirtualColumn(/*column=*/kColumn1Column, /*step=*/2, /*row_offset=*/64));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y", VirtualColumn(/*column=*/kColumn1Column, /*step=*/2, /*row_offset=*/96));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking192",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/128, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking193",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/128, /*row_offset=*/65));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking194",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/128, /*row_offset=*/33));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking195",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/128, /*row_offset=*/97));

  ctx.AddPeriodicColumn(
      "pedersen/points/x",
      VirtualColumn(/*column=*/kPedersenPointsXPeriodicColumn, /*step=*/4, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "pedersen/points/y",
      VirtualColumn(/*column=*/kPedersenPointsYPeriodicColumn, /*step=*/4, /*row_offset=*/0));

  ctx.AddObject<std::vector<size_t>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);

  return ctx;
}

template <typename FieldElementT>
std::vector<std::pair<int64_t, uint64_t>> CpuAirDefinition<FieldElementT, 4>::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;

  mask.reserve(133);
  mask.emplace_back(0, kColumn0Column);
  mask.emplace_back(1, kColumn0Column);
  mask.emplace_back(2, kColumn0Column);
  mask.emplace_back(3, kColumn0Column);
  mask.emplace_back(4, kColumn0Column);
  mask.emplace_back(5, kColumn0Column);
  mask.emplace_back(6, kColumn0Column);
  mask.emplace_back(7, kColumn0Column);
  mask.emplace_back(8, kColumn0Column);
  mask.emplace_back(9, kColumn0Column);
  mask.emplace_back(10, kColumn0Column);
  mask.emplace_back(11, kColumn0Column);
  mask.emplace_back(12, kColumn0Column);
  mask.emplace_back(13, kColumn0Column);
  mask.emplace_back(14, kColumn0Column);
  mask.emplace_back(15, kColumn0Column);
  mask.emplace_back(0, kColumn1Column);
  mask.emplace_back(1, kColumn1Column);
  mask.emplace_back(2, kColumn1Column);
  mask.emplace_back(4, kColumn1Column);
  mask.emplace_back(6, kColumn1Column);
  mask.emplace_back(8, kColumn1Column);
  mask.emplace_back(10, kColumn1Column);
  mask.emplace_back(12, kColumn1Column);
  mask.emplace_back(14, kColumn1Column);
  mask.emplace_back(16, kColumn1Column);
  mask.emplace_back(18, kColumn1Column);
  mask.emplace_back(20, kColumn1Column);
  mask.emplace_back(22, kColumn1Column);
  mask.emplace_back(24, kColumn1Column);
  mask.emplace_back(26, kColumn1Column);
  mask.emplace_back(28, kColumn1Column);
  mask.emplace_back(30, kColumn1Column);
  mask.emplace_back(32, kColumn1Column);
  mask.emplace_back(33, kColumn1Column);
  mask.emplace_back(64, kColumn1Column);
  mask.emplace_back(65, kColumn1Column);
  mask.emplace_back(88, kColumn1Column);
  mask.emplace_back(90, kColumn1Column);
  mask.emplace_back(92, kColumn1Column);
  mask.emplace_back(94, kColumn1Column);
  mask.emplace_back(96, kColumn1Column);
  mask.emplace_back(97, kColumn1Column);
  mask.emplace_back(120, kColumn1Column);
  mask.emplace_back(122, kColumn1Column);
  mask.emplace_back(124, kColumn1Column);
  mask.emplace_back(126, kColumn1Column);
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
  mask.emplace_back(10, kColumn3Column);
  mask.emplace_back(11, kColumn3Column);
  mask.emplace_back(12, kColumn3Column);
  mask.emplace_back(13, kColumn3Column);
  mask.emplace_back(16, kColumn3Column);
  mask.emplace_back(26, kColumn3Column);
  mask.emplace_back(27, kColumn3Column);
  mask.emplace_back(42, kColumn3Column);
  mask.emplace_back(43, kColumn3Column);
  mask.emplace_back(58, kColumn3Column);
  mask.emplace_back(74, kColumn3Column);
  mask.emplace_back(75, kColumn3Column);
  mask.emplace_back(91, kColumn3Column);
  mask.emplace_back(122, kColumn3Column);
  mask.emplace_back(123, kColumn3Column);
  mask.emplace_back(154, kColumn3Column);
  mask.emplace_back(202, kColumn3Column);
  mask.emplace_back(522, kColumn3Column);
  mask.emplace_back(523, kColumn3Column);
  mask.emplace_back(1034, kColumn3Column);
  mask.emplace_back(1035, kColumn3Column);
  mask.emplace_back(2058, kColumn3Column);
  mask.emplace_back(0, kColumn4Column);
  mask.emplace_back(1, kColumn4Column);
  mask.emplace_back(2, kColumn4Column);
  mask.emplace_back(3, kColumn4Column);
  mask.emplace_back(0, kColumn5Column);
  mask.emplace_back(1, kColumn5Column);
  mask.emplace_back(2, kColumn5Column);
  mask.emplace_back(3, kColumn5Column);
  mask.emplace_back(4, kColumn5Column);
  mask.emplace_back(5, kColumn5Column);
  mask.emplace_back(6, kColumn5Column);
  mask.emplace_back(7, kColumn5Column);
  mask.emplace_back(8, kColumn5Column);
  mask.emplace_back(12, kColumn5Column);
  mask.emplace_back(28, kColumn5Column);
  mask.emplace_back(44, kColumn5Column);
  mask.emplace_back(60, kColumn5Column);
  mask.emplace_back(76, kColumn5Column);
  mask.emplace_back(92, kColumn5Column);
  mask.emplace_back(108, kColumn5Column);
  mask.emplace_back(124, kColumn5Column);
  mask.emplace_back(1021, kColumn5Column);
  mask.emplace_back(1023, kColumn5Column);
  mask.emplace_back(1025, kColumn5Column);
  mask.emplace_back(1027, kColumn5Column);
  mask.emplace_back(2045, kColumn5Column);
  mask.emplace_back(0, kColumn6Column);
  mask.emplace_back(1, kColumn6Column);
  mask.emplace_back(2, kColumn6Column);
  mask.emplace_back(3, kColumn6Column);
  mask.emplace_back(4, kColumn6Column);
  mask.emplace_back(5, kColumn6Column);
  mask.emplace_back(7, kColumn6Column);
  mask.emplace_back(9, kColumn6Column);
  mask.emplace_back(11, kColumn6Column);
  mask.emplace_back(13, kColumn6Column);
  mask.emplace_back(17, kColumn6Column);
  mask.emplace_back(25, kColumn6Column);
  mask.emplace_back(768, kColumn6Column);
  mask.emplace_back(772, kColumn6Column);
  mask.emplace_back(784, kColumn6Column);
  mask.emplace_back(788, kColumn6Column);
  mask.emplace_back(1004, kColumn6Column);
  mask.emplace_back(1008, kColumn6Column);
  mask.emplace_back(1022, kColumn6Column);
  mask.emplace_back(1024, kColumn6Column);
  mask.emplace_back(0, kColumn7Inter1Column);
  mask.emplace_back(1, kColumn7Inter1Column);
  mask.emplace_back(0, kColumn8Inter1Column);
  mask.emplace_back(1, kColumn8Inter1Column);
  mask.emplace_back(0, kColumn9Inter1Column);
  mask.emplace_back(1, kColumn9Inter1Column);
  mask.emplace_back(2, kColumn9Inter1Column);
  mask.emplace_back(5, kColumn9Inter1Column);

  return mask;
}

}  // namespace cpu

}  // namespace starkware
