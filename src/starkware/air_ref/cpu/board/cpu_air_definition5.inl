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
CpuAirDefinition<FieldElementT, 5>::CreateCompositionPolynomial(
    const FieldElement& trace_generator, const ConstFieldElementSpan& random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const FieldElementT& gen = trace_generator.As<FieldElementT>();

  const std::vector<uint64_t> point_exponents = {
      trace_length_,
      SafeDiv(trace_length_, 2),
      SafeDiv(trace_length_, 4),
      SafeDiv(trace_length_, 8),
      SafeDiv(trace_length_, 16),
      SafeDiv(trace_length_, 32),
      SafeDiv(trace_length_, 128),
      SafeDiv(trace_length_, 256),
      SafeDiv(trace_length_, 512),
      SafeDiv(trace_length_, 1024),
      SafeDiv(trace_length_, 4096),
      SafeDiv(trace_length_, 8192)};
  const std::vector<uint64_t> gen_exponents = {
      SafeDiv((15) * (trace_length_), 16),
      SafeDiv((255) * (trace_length_), 256),
      SafeDiv((63) * (trace_length_), 64),
      SafeDiv(trace_length_, 2),
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
      SafeDiv((251) * (trace_length_), 256),
      (16) * ((SafeDiv(trace_length_, 16)) - (1)),
      (2) * ((SafeDiv(trace_length_, 2)) - (1)),
      (8) * ((SafeDiv(trace_length_, 8)) - (1)),
      (4) * ((SafeDiv(trace_length_, 4)) - (1)),
      (128) * ((SafeDiv(trace_length_, 128)) - (1)),
      (8192) * ((SafeDiv(trace_length_, 8192)) - (1)),
      (1024) * ((SafeDiv(trace_length_, 1024)) - (1))};

  BuildPeriodicColumns(gen, &builder);

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
CpuAirDefinition<FieldElementT, 5>::PrecomputeDomainEvalsOnCoset(
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
      FieldElementT::UninitializedVector(4),    FieldElementT::UninitializedVector(8),
      FieldElementT::UninitializedVector(16),   FieldElementT::UninitializedVector(16),
      FieldElementT::UninitializedVector(32),   FieldElementT::UninitializedVector(128),
      FieldElementT::UninitializedVector(256),  FieldElementT::UninitializedVector(256),
      FieldElementT::UninitializedVector(256),  FieldElementT::UninitializedVector(512),
      FieldElementT::UninitializedVector(512),  FieldElementT::UninitializedVector(1024),
      FieldElementT::UninitializedVector(1024), FieldElementT::UninitializedVector(1024),
      FieldElementT::UninitializedVector(4096), FieldElementT::UninitializedVector(4096),
      FieldElementT::UninitializedVector(4096), FieldElementT::UninitializedVector(8192),
      FieldElementT::UninitializedVector(8192), FieldElementT::UninitializedVector(8192),
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

  period = 8;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[3][i] = (point_powers[3][i & (7)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 16;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[4][i] = (point_powers[4][i & (15)]) - (shifts[0]);
        }
      },
      period, kTaskSize);

  period = 16;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[5][i] = (point_powers[4][i & (15)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 32;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[6][i] = (point_powers[5][i & (31)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[7][i] = (point_powers[6][i & (127)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[8][i] = (point_powers[7][i & (255)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[9][i] = (point_powers[7][i & (255)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[10][i] = (point_powers[7][i & (255)]) - (shifts[2]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[11][i] = (point_powers[8][i & (511)]) - (shifts[3]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[12][i] = (point_powers[8][i & (511)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[13][i] = (point_powers[9][i & (1023)]) - (shifts[4]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[14][i] = (point_powers[9][i & (1023)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[15][i] = ((point_powers[9][i & (1023)]) - (shifts[5])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[6])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[7])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[8])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[9])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[10])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[11])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[12])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[13])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[14])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[15])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[16])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[17])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[18])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[19])) *
                                   (precomp_domains[14][i & (1024 - 1)]);
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[16][i] = (point_powers[10][i & (4095)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[17][i] = (point_powers[10][i & (4095)]) - (shifts[20]);
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[18][i] = (point_powers[10][i & (4095)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 8192;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[19][i] = (point_powers[11][i & (8191)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 8192;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[20][i] = (point_powers[11][i & (8191)]) - (shifts[20]);
        }
      },
      period, kTaskSize);

  period = 8192;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[21][i] = (point_powers[11][i & (8191)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);
  return precomp_domains;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> CpuAirDefinition<FieldElementT, 5>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 28, "shifts should contain 28 elements.");

  // domain0 = point^trace_length - 1.
  const FieldElementT& domain0 = precomp_domains[0];
  // domain1 = point^(trace_length / 2) - 1.
  const FieldElementT& domain1 = precomp_domains[1];
  // domain2 = point^(trace_length / 4) - 1.
  const FieldElementT& domain2 = precomp_domains[2];
  // domain3 = point^(trace_length / 8) - 1.
  const FieldElementT& domain3 = precomp_domains[3];
  // domain4 = point^(trace_length / 16) - gen^(15 * trace_length / 16).
  const FieldElementT& domain4 = precomp_domains[4];
  // domain5 = point^(trace_length / 16) - 1.
  const FieldElementT& domain5 = precomp_domains[5];
  // domain6 = point^(trace_length / 32) - 1.
  const FieldElementT& domain6 = precomp_domains[6];
  // domain7 = point^(trace_length / 128) - 1.
  const FieldElementT& domain7 = precomp_domains[7];
  // domain8 = point^(trace_length / 256) - gen^(255 * trace_length / 256).
  const FieldElementT& domain8 = precomp_domains[8];
  // domain9 = point^(trace_length / 256) - 1.
  const FieldElementT& domain9 = precomp_domains[9];
  // domain10 = point^(trace_length / 256) - gen^(63 * trace_length / 64).
  const FieldElementT& domain10 = precomp_domains[10];
  // domain11 = point^(trace_length / 512) - gen^(trace_length / 2).
  const FieldElementT& domain11 = precomp_domains[11];
  // domain12 = point^(trace_length / 512) - 1.
  const FieldElementT& domain12 = precomp_domains[12];
  // domain13 = point^(trace_length / 1024) - gen^(3 * trace_length / 4).
  const FieldElementT& domain13 = precomp_domains[13];
  // domain14 = point^(trace_length / 1024) - 1.
  const FieldElementT& domain14 = precomp_domains[14];
  // domain15 = (point^(trace_length / 1024) - gen^(trace_length / 64)) * (point^(trace_length /
  // 1024) - gen^(trace_length / 32)) * (point^(trace_length / 1024) - gen^(3 * trace_length / 64))
  // * (point^(trace_length / 1024) - gen^(trace_length / 16)) * (point^(trace_length / 1024) -
  // gen^(5 * trace_length / 64)) * (point^(trace_length / 1024) - gen^(3 * trace_length / 32)) *
  // (point^(trace_length / 1024) - gen^(7 * trace_length / 64)) * (point^(trace_length / 1024) -
  // gen^(trace_length / 8)) * (point^(trace_length / 1024) - gen^(9 * trace_length / 64)) *
  // (point^(trace_length / 1024) - gen^(5 * trace_length / 32)) * (point^(trace_length / 1024) -
  // gen^(11 * trace_length / 64)) * (point^(trace_length / 1024) - gen^(3 * trace_length / 16)) *
  // (point^(trace_length / 1024) - gen^(13 * trace_length / 64)) * (point^(trace_length / 1024) -
  // gen^(7 * trace_length / 32)) * (point^(trace_length / 1024) - gen^(15 * trace_length / 64)) *
  // domain14.
  const FieldElementT& domain15 = precomp_domains[15];
  // domain16 = point^(trace_length / 4096) - gen^(255 * trace_length / 256).
  const FieldElementT& domain16 = precomp_domains[16];
  // domain17 = point^(trace_length / 4096) - gen^(251 * trace_length / 256).
  const FieldElementT& domain17 = precomp_domains[17];
  // domain18 = point^(trace_length / 4096) - 1.
  const FieldElementT& domain18 = precomp_domains[18];
  // domain19 = point^(trace_length / 8192) - gen^(255 * trace_length / 256).
  const FieldElementT& domain19 = precomp_domains[19];
  // domain20 = point^(trace_length / 8192) - gen^(251 * trace_length / 256).
  const FieldElementT& domain20 = precomp_domains[20];
  // domain21 = point^(trace_length / 8192) - 1.
  const FieldElementT& domain21 = precomp_domains[21];
  // domain22 = point - gen^(16 * (trace_length / 16 - 1)).
  const FieldElementT& domain22 = (point) - (shifts[21]);
  // domain23 = point - 1.
  const FieldElementT& domain23 = (point) - (FieldElementT::One());
  // domain24 = point - gen^(2 * (trace_length / 2 - 1)).
  const FieldElementT& domain24 = (point) - (shifts[22]);
  // domain25 = point - gen^(8 * (trace_length / 8 - 1)).
  const FieldElementT& domain25 = (point) - (shifts[23]);
  // domain26 = point - gen^(4 * (trace_length / 4 - 1)).
  const FieldElementT& domain26 = (point) - (shifts[24]);
  // domain27 = point - gen^(128 * (trace_length / 128 - 1)).
  const FieldElementT& domain27 = (point) - (shifts[25]);
  // domain28 = point - gen^(8192 * (trace_length / 8192 - 1)).
  const FieldElementT& domain28 = (point) - (shifts[26]);
  // domain29 = point - gen^(1024 * (trace_length / 1024 - 1)).
  const FieldElementT& domain29 = (point) - (shifts[27]);

  ASSERT_VERIFIER(neighbors.size() == 246, "Neighbors must contain 246 elements.");
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
  const FieldElementT& column1_row255 = neighbors[kColumn1Row255Neighbor];
  const FieldElementT& column1_row256 = neighbors[kColumn1Row256Neighbor];
  const FieldElementT& column1_row511 = neighbors[kColumn1Row511Neighbor];
  const FieldElementT& column2_row0 = neighbors[kColumn2Row0Neighbor];
  const FieldElementT& column2_row1 = neighbors[kColumn2Row1Neighbor];
  const FieldElementT& column2_row255 = neighbors[kColumn2Row255Neighbor];
  const FieldElementT& column2_row256 = neighbors[kColumn2Row256Neighbor];
  const FieldElementT& column3_row0 = neighbors[kColumn3Row0Neighbor];
  const FieldElementT& column3_row1 = neighbors[kColumn3Row1Neighbor];
  const FieldElementT& column3_row192 = neighbors[kColumn3Row192Neighbor];
  const FieldElementT& column3_row193 = neighbors[kColumn3Row193Neighbor];
  const FieldElementT& column3_row196 = neighbors[kColumn3Row196Neighbor];
  const FieldElementT& column3_row197 = neighbors[kColumn3Row197Neighbor];
  const FieldElementT& column3_row251 = neighbors[kColumn3Row251Neighbor];
  const FieldElementT& column3_row252 = neighbors[kColumn3Row252Neighbor];
  const FieldElementT& column3_row256 = neighbors[kColumn3Row256Neighbor];
  const FieldElementT& column4_row0 = neighbors[kColumn4Row0Neighbor];
  const FieldElementT& column4_row1 = neighbors[kColumn4Row1Neighbor];
  const FieldElementT& column4_row255 = neighbors[kColumn4Row255Neighbor];
  const FieldElementT& column4_row256 = neighbors[kColumn4Row256Neighbor];
  const FieldElementT& column4_row511 = neighbors[kColumn4Row511Neighbor];
  const FieldElementT& column5_row0 = neighbors[kColumn5Row0Neighbor];
  const FieldElementT& column5_row1 = neighbors[kColumn5Row1Neighbor];
  const FieldElementT& column5_row255 = neighbors[kColumn5Row255Neighbor];
  const FieldElementT& column5_row256 = neighbors[kColumn5Row256Neighbor];
  const FieldElementT& column6_row0 = neighbors[kColumn6Row0Neighbor];
  const FieldElementT& column6_row1 = neighbors[kColumn6Row1Neighbor];
  const FieldElementT& column6_row192 = neighbors[kColumn6Row192Neighbor];
  const FieldElementT& column6_row193 = neighbors[kColumn6Row193Neighbor];
  const FieldElementT& column6_row196 = neighbors[kColumn6Row196Neighbor];
  const FieldElementT& column6_row197 = neighbors[kColumn6Row197Neighbor];
  const FieldElementT& column6_row251 = neighbors[kColumn6Row251Neighbor];
  const FieldElementT& column6_row252 = neighbors[kColumn6Row252Neighbor];
  const FieldElementT& column6_row256 = neighbors[kColumn6Row256Neighbor];
  const FieldElementT& column7_row0 = neighbors[kColumn7Row0Neighbor];
  const FieldElementT& column7_row1 = neighbors[kColumn7Row1Neighbor];
  const FieldElementT& column7_row255 = neighbors[kColumn7Row255Neighbor];
  const FieldElementT& column7_row256 = neighbors[kColumn7Row256Neighbor];
  const FieldElementT& column7_row511 = neighbors[kColumn7Row511Neighbor];
  const FieldElementT& column8_row0 = neighbors[kColumn8Row0Neighbor];
  const FieldElementT& column8_row1 = neighbors[kColumn8Row1Neighbor];
  const FieldElementT& column8_row255 = neighbors[kColumn8Row255Neighbor];
  const FieldElementT& column8_row256 = neighbors[kColumn8Row256Neighbor];
  const FieldElementT& column9_row0 = neighbors[kColumn9Row0Neighbor];
  const FieldElementT& column9_row1 = neighbors[kColumn9Row1Neighbor];
  const FieldElementT& column9_row192 = neighbors[kColumn9Row192Neighbor];
  const FieldElementT& column9_row193 = neighbors[kColumn9Row193Neighbor];
  const FieldElementT& column9_row196 = neighbors[kColumn9Row196Neighbor];
  const FieldElementT& column9_row197 = neighbors[kColumn9Row197Neighbor];
  const FieldElementT& column9_row251 = neighbors[kColumn9Row251Neighbor];
  const FieldElementT& column9_row252 = neighbors[kColumn9Row252Neighbor];
  const FieldElementT& column9_row256 = neighbors[kColumn9Row256Neighbor];
  const FieldElementT& column10_row0 = neighbors[kColumn10Row0Neighbor];
  const FieldElementT& column10_row1 = neighbors[kColumn10Row1Neighbor];
  const FieldElementT& column10_row255 = neighbors[kColumn10Row255Neighbor];
  const FieldElementT& column10_row256 = neighbors[kColumn10Row256Neighbor];
  const FieldElementT& column10_row511 = neighbors[kColumn10Row511Neighbor];
  const FieldElementT& column11_row0 = neighbors[kColumn11Row0Neighbor];
  const FieldElementT& column11_row1 = neighbors[kColumn11Row1Neighbor];
  const FieldElementT& column11_row255 = neighbors[kColumn11Row255Neighbor];
  const FieldElementT& column11_row256 = neighbors[kColumn11Row256Neighbor];
  const FieldElementT& column12_row0 = neighbors[kColumn12Row0Neighbor];
  const FieldElementT& column12_row1 = neighbors[kColumn12Row1Neighbor];
  const FieldElementT& column12_row192 = neighbors[kColumn12Row192Neighbor];
  const FieldElementT& column12_row193 = neighbors[kColumn12Row193Neighbor];
  const FieldElementT& column12_row196 = neighbors[kColumn12Row196Neighbor];
  const FieldElementT& column12_row197 = neighbors[kColumn12Row197Neighbor];
  const FieldElementT& column12_row251 = neighbors[kColumn12Row251Neighbor];
  const FieldElementT& column12_row252 = neighbors[kColumn12Row252Neighbor];
  const FieldElementT& column12_row256 = neighbors[kColumn12Row256Neighbor];
  const FieldElementT& column13_row0 = neighbors[kColumn13Row0Neighbor];
  const FieldElementT& column13_row255 = neighbors[kColumn13Row255Neighbor];
  const FieldElementT& column14_row0 = neighbors[kColumn14Row0Neighbor];
  const FieldElementT& column14_row255 = neighbors[kColumn14Row255Neighbor];
  const FieldElementT& column15_row0 = neighbors[kColumn15Row0Neighbor];
  const FieldElementT& column15_row255 = neighbors[kColumn15Row255Neighbor];
  const FieldElementT& column16_row0 = neighbors[kColumn16Row0Neighbor];
  const FieldElementT& column16_row255 = neighbors[kColumn16Row255Neighbor];
  const FieldElementT& column17_row0 = neighbors[kColumn17Row0Neighbor];
  const FieldElementT& column17_row1 = neighbors[kColumn17Row1Neighbor];
  const FieldElementT& column17_row2 = neighbors[kColumn17Row2Neighbor];
  const FieldElementT& column17_row3 = neighbors[kColumn17Row3Neighbor];
  const FieldElementT& column17_row4 = neighbors[kColumn17Row4Neighbor];
  const FieldElementT& column17_row5 = neighbors[kColumn17Row5Neighbor];
  const FieldElementT& column17_row6 = neighbors[kColumn17Row6Neighbor];
  const FieldElementT& column17_row7 = neighbors[kColumn17Row7Neighbor];
  const FieldElementT& column17_row8 = neighbors[kColumn17Row8Neighbor];
  const FieldElementT& column17_row9 = neighbors[kColumn17Row9Neighbor];
  const FieldElementT& column17_row12 = neighbors[kColumn17Row12Neighbor];
  const FieldElementT& column17_row13 = neighbors[kColumn17Row13Neighbor];
  const FieldElementT& column17_row16 = neighbors[kColumn17Row16Neighbor];
  const FieldElementT& column17_row22 = neighbors[kColumn17Row22Neighbor];
  const FieldElementT& column17_row23 = neighbors[kColumn17Row23Neighbor];
  const FieldElementT& column17_row38 = neighbors[kColumn17Row38Neighbor];
  const FieldElementT& column17_row39 = neighbors[kColumn17Row39Neighbor];
  const FieldElementT& column17_row70 = neighbors[kColumn17Row70Neighbor];
  const FieldElementT& column17_row71 = neighbors[kColumn17Row71Neighbor];
  const FieldElementT& column17_row102 = neighbors[kColumn17Row102Neighbor];
  const FieldElementT& column17_row103 = neighbors[kColumn17Row103Neighbor];
  const FieldElementT& column17_row134 = neighbors[kColumn17Row134Neighbor];
  const FieldElementT& column17_row135 = neighbors[kColumn17Row135Neighbor];
  const FieldElementT& column17_row150 = neighbors[kColumn17Row150Neighbor];
  const FieldElementT& column17_row151 = neighbors[kColumn17Row151Neighbor];
  const FieldElementT& column17_row167 = neighbors[kColumn17Row167Neighbor];
  const FieldElementT& column17_row199 = neighbors[kColumn17Row199Neighbor];
  const FieldElementT& column17_row230 = neighbors[kColumn17Row230Neighbor];
  const FieldElementT& column17_row263 = neighbors[kColumn17Row263Neighbor];
  const FieldElementT& column17_row295 = neighbors[kColumn17Row295Neighbor];
  const FieldElementT& column17_row327 = neighbors[kColumn17Row327Neighbor];
  const FieldElementT& column17_row391 = neighbors[kColumn17Row391Neighbor];
  const FieldElementT& column17_row406 = neighbors[kColumn17Row406Neighbor];
  const FieldElementT& column17_row423 = neighbors[kColumn17Row423Neighbor];
  const FieldElementT& column17_row455 = neighbors[kColumn17Row455Neighbor];
  const FieldElementT& column17_row534 = neighbors[kColumn17Row534Neighbor];
  const FieldElementT& column17_row535 = neighbors[kColumn17Row535Neighbor];
  const FieldElementT& column17_row663 = neighbors[kColumn17Row663Neighbor];
  const FieldElementT& column17_row918 = neighbors[kColumn17Row918Neighbor];
  const FieldElementT& column17_row919 = neighbors[kColumn17Row919Neighbor];
  const FieldElementT& column17_row1174 = neighbors[kColumn17Row1174Neighbor];
  const FieldElementT& column17_row4118 = neighbors[kColumn17Row4118Neighbor];
  const FieldElementT& column17_row4119 = neighbors[kColumn17Row4119Neighbor];
  const FieldElementT& column17_row8214 = neighbors[kColumn17Row8214Neighbor];
  const FieldElementT& column18_row0 = neighbors[kColumn18Row0Neighbor];
  const FieldElementT& column18_row1 = neighbors[kColumn18Row1Neighbor];
  const FieldElementT& column18_row2 = neighbors[kColumn18Row2Neighbor];
  const FieldElementT& column18_row3 = neighbors[kColumn18Row3Neighbor];
  const FieldElementT& column19_row0 = neighbors[kColumn19Row0Neighbor];
  const FieldElementT& column19_row1 = neighbors[kColumn19Row1Neighbor];
  const FieldElementT& column19_row2 = neighbors[kColumn19Row2Neighbor];
  const FieldElementT& column19_row3 = neighbors[kColumn19Row3Neighbor];
  const FieldElementT& column19_row4 = neighbors[kColumn19Row4Neighbor];
  const FieldElementT& column19_row5 = neighbors[kColumn19Row5Neighbor];
  const FieldElementT& column19_row6 = neighbors[kColumn19Row6Neighbor];
  const FieldElementT& column19_row7 = neighbors[kColumn19Row7Neighbor];
  const FieldElementT& column19_row8 = neighbors[kColumn19Row8Neighbor];
  const FieldElementT& column19_row9 = neighbors[kColumn19Row9Neighbor];
  const FieldElementT& column19_row11 = neighbors[kColumn19Row11Neighbor];
  const FieldElementT& column19_row12 = neighbors[kColumn19Row12Neighbor];
  const FieldElementT& column19_row13 = neighbors[kColumn19Row13Neighbor];
  const FieldElementT& column19_row15 = neighbors[kColumn19Row15Neighbor];
  const FieldElementT& column19_row17 = neighbors[kColumn19Row17Neighbor];
  const FieldElementT& column19_row19 = neighbors[kColumn19Row19Neighbor];
  const FieldElementT& column19_row27 = neighbors[kColumn19Row27Neighbor];
  const FieldElementT& column19_row28 = neighbors[kColumn19Row28Neighbor];
  const FieldElementT& column19_row33 = neighbors[kColumn19Row33Neighbor];
  const FieldElementT& column19_row44 = neighbors[kColumn19Row44Neighbor];
  const FieldElementT& column19_row49 = neighbors[kColumn19Row49Neighbor];
  const FieldElementT& column19_row60 = neighbors[kColumn19Row60Neighbor];
  const FieldElementT& column19_row65 = neighbors[kColumn19Row65Neighbor];
  const FieldElementT& column19_row76 = neighbors[kColumn19Row76Neighbor];
  const FieldElementT& column19_row81 = neighbors[kColumn19Row81Neighbor];
  const FieldElementT& column19_row92 = neighbors[kColumn19Row92Neighbor];
  const FieldElementT& column19_row97 = neighbors[kColumn19Row97Neighbor];
  const FieldElementT& column19_row108 = neighbors[kColumn19Row108Neighbor];
  const FieldElementT& column19_row113 = neighbors[kColumn19Row113Neighbor];
  const FieldElementT& column19_row124 = neighbors[kColumn19Row124Neighbor];
  const FieldElementT& column19_row129 = neighbors[kColumn19Row129Neighbor];
  const FieldElementT& column19_row145 = neighbors[kColumn19Row145Neighbor];
  const FieldElementT& column19_row161 = neighbors[kColumn19Row161Neighbor];
  const FieldElementT& column19_row177 = neighbors[kColumn19Row177Neighbor];
  const FieldElementT& column19_row193 = neighbors[kColumn19Row193Neighbor];
  const FieldElementT& column19_row209 = neighbors[kColumn19Row209Neighbor];
  const FieldElementT& column19_row225 = neighbors[kColumn19Row225Neighbor];
  const FieldElementT& column19_row241 = neighbors[kColumn19Row241Neighbor];
  const FieldElementT& column19_row257 = neighbors[kColumn19Row257Neighbor];
  const FieldElementT& column19_row265 = neighbors[kColumn19Row265Neighbor];
  const FieldElementT& column19_row513 = neighbors[kColumn19Row513Neighbor];
  const FieldElementT& column19_row521 = neighbors[kColumn19Row521Neighbor];
  const FieldElementT& column19_row705 = neighbors[kColumn19Row705Neighbor];
  const FieldElementT& column19_row721 = neighbors[kColumn19Row721Neighbor];
  const FieldElementT& column19_row737 = neighbors[kColumn19Row737Neighbor];
  const FieldElementT& column19_row753 = neighbors[kColumn19Row753Neighbor];
  const FieldElementT& column19_row769 = neighbors[kColumn19Row769Neighbor];
  const FieldElementT& column19_row777 = neighbors[kColumn19Row777Neighbor];
  const FieldElementT& column19_row961 = neighbors[kColumn19Row961Neighbor];
  const FieldElementT& column19_row977 = neighbors[kColumn19Row977Neighbor];
  const FieldElementT& column19_row993 = neighbors[kColumn19Row993Neighbor];
  const FieldElementT& column19_row1009 = neighbors[kColumn19Row1009Neighbor];
  const FieldElementT& column20_row0 = neighbors[kColumn20Row0Neighbor];
  const FieldElementT& column20_row1 = neighbors[kColumn20Row1Neighbor];
  const FieldElementT& column20_row2 = neighbors[kColumn20Row2Neighbor];
  const FieldElementT& column20_row3 = neighbors[kColumn20Row3Neighbor];
  const FieldElementT& column20_row4 = neighbors[kColumn20Row4Neighbor];
  const FieldElementT& column20_row5 = neighbors[kColumn20Row5Neighbor];
  const FieldElementT& column20_row6 = neighbors[kColumn20Row6Neighbor];
  const FieldElementT& column20_row8 = neighbors[kColumn20Row8Neighbor];
  const FieldElementT& column20_row9 = neighbors[kColumn20Row9Neighbor];
  const FieldElementT& column20_row10 = neighbors[kColumn20Row10Neighbor];
  const FieldElementT& column20_row12 = neighbors[kColumn20Row12Neighbor];
  const FieldElementT& column20_row13 = neighbors[kColumn20Row13Neighbor];
  const FieldElementT& column20_row14 = neighbors[kColumn20Row14Neighbor];
  const FieldElementT& column20_row18 = neighbors[kColumn20Row18Neighbor];
  const FieldElementT& column20_row19 = neighbors[kColumn20Row19Neighbor];
  const FieldElementT& column20_row20 = neighbors[kColumn20Row20Neighbor];
  const FieldElementT& column20_row21 = neighbors[kColumn20Row21Neighbor];
  const FieldElementT& column20_row22 = neighbors[kColumn20Row22Neighbor];
  const FieldElementT& column20_row26 = neighbors[kColumn20Row26Neighbor];
  const FieldElementT& column20_row28 = neighbors[kColumn20Row28Neighbor];
  const FieldElementT& column20_row29 = neighbors[kColumn20Row29Neighbor];
  const FieldElementT& column20_row37 = neighbors[kColumn20Row37Neighbor];
  const FieldElementT& column20_row45 = neighbors[kColumn20Row45Neighbor];
  const FieldElementT& column20_row53 = neighbors[kColumn20Row53Neighbor];
  const FieldElementT& column20_row83 = neighbors[kColumn20Row83Neighbor];
  const FieldElementT& column20_row147 = neighbors[kColumn20Row147Neighbor];
  const FieldElementT& column20_row211 = neighbors[kColumn20Row211Neighbor];
  const FieldElementT& column20_row4081 = neighbors[kColumn20Row4081Neighbor];
  const FieldElementT& column20_row4082 = neighbors[kColumn20Row4082Neighbor];
  const FieldElementT& column20_row4089 = neighbors[kColumn20Row4089Neighbor];
  const FieldElementT& column20_row4090 = neighbors[kColumn20Row4090Neighbor];
  const FieldElementT& column20_row4094 = neighbors[kColumn20Row4094Neighbor];
  const FieldElementT& column20_row4100 = neighbors[kColumn20Row4100Neighbor];
  const FieldElementT& column20_row4108 = neighbors[kColumn20Row4108Neighbor];
  const FieldElementT& column20_row8163 = neighbors[kColumn20Row8163Neighbor];
  const FieldElementT& column20_row8165 = neighbors[kColumn20Row8165Neighbor];
  const FieldElementT& column20_row8177 = neighbors[kColumn20Row8177Neighbor];
  const FieldElementT& column20_row8178 = neighbors[kColumn20Row8178Neighbor];
  const FieldElementT& column20_row8181 = neighbors[kColumn20Row8181Neighbor];
  const FieldElementT& column20_row8185 = neighbors[kColumn20Row8185Neighbor];
  const FieldElementT& column20_row8186 = neighbors[kColumn20Row8186Neighbor];
  const FieldElementT& column20_row8189 = neighbors[kColumn20Row8189Neighbor];
  const FieldElementT& column21_inter1_row0 = neighbors[kColumn21Inter1Row0Neighbor];
  const FieldElementT& column21_inter1_row1 = neighbors[kColumn21Inter1Row1Neighbor];
  const FieldElementT& column21_inter1_row2 = neighbors[kColumn21Inter1Row2Neighbor];
  const FieldElementT& column21_inter1_row3 = neighbors[kColumn21Inter1Row3Neighbor];
  const FieldElementT& column21_inter1_row5 = neighbors[kColumn21Inter1Row5Neighbor];
  const FieldElementT& column21_inter1_row7 = neighbors[kColumn21Inter1Row7Neighbor];
  const FieldElementT& column21_inter1_row11 = neighbors[kColumn21Inter1Row11Neighbor];
  const FieldElementT& column21_inter1_row15 = neighbors[kColumn21Inter1Row15Neighbor];

  ASSERT_VERIFIER(periodic_columns.size() == 4, "periodic_columns should contain 4 elements.");
  const FieldElementT& pedersen__points__x = periodic_columns[kPedersenPointsXPeriodicColumn];
  const FieldElementT& pedersen__points__y = periodic_columns[kPedersenPointsYPeriodicColumn];
  const FieldElementT& ecdsa__generator_points__x =
      periodic_columns[kEcdsaGeneratorPointsXPeriodicColumn];
  const FieldElementT& ecdsa__generator_points__y =
      periodic_columns[kEcdsaGeneratorPointsYPeriodicColumn];

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
      ((column17_row0) + (cpu__decode__opcode_rc__bit_2)) + (FieldElementT::One());
  const FieldElementT cpu__decode__opcode_rc__bit_10 =
      (column0_row10) - ((column0_row11) + (column0_row11));
  const FieldElementT cpu__decode__opcode_rc__bit_11 =
      (column0_row11) - ((column0_row12) + (column0_row12));
  const FieldElementT cpu__decode__opcode_rc__bit_14 =
      (column0_row14) - ((column0_row15) + (column0_row15));
  const FieldElementT memory__address_diff_0 = (column18_row2) - (column18_row0);
  const FieldElementT rc16__diff_0 = (column19_row6) - (column19_row2);
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_0 =
      (column3_row0) - ((column3_row1) + (column3_row1));
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash0__ec_subset_sum__bit_0);
  const FieldElementT pedersen__hash1__ec_subset_sum__bit_0 =
      (column6_row0) - ((column6_row1) + (column6_row1));
  const FieldElementT pedersen__hash1__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash1__ec_subset_sum__bit_0);
  const FieldElementT pedersen__hash2__ec_subset_sum__bit_0 =
      (column9_row0) - ((column9_row1) + (column9_row1));
  const FieldElementT pedersen__hash2__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash2__ec_subset_sum__bit_0);
  const FieldElementT pedersen__hash3__ec_subset_sum__bit_0 =
      (column12_row0) - ((column12_row1) + (column12_row1));
  const FieldElementT pedersen__hash3__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash3__ec_subset_sum__bit_0);
  const FieldElementT rc_builtin__value0_0 = column19_row12;
  const FieldElementT rc_builtin__value1_0 =
      ((rc_builtin__value0_0) * (offset_size_)) + (column19_row28);
  const FieldElementT rc_builtin__value2_0 =
      ((rc_builtin__value1_0) * (offset_size_)) + (column19_row44);
  const FieldElementT rc_builtin__value3_0 =
      ((rc_builtin__value2_0) * (offset_size_)) + (column19_row60);
  const FieldElementT rc_builtin__value4_0 =
      ((rc_builtin__value3_0) * (offset_size_)) + (column19_row76);
  const FieldElementT rc_builtin__value5_0 =
      ((rc_builtin__value4_0) * (offset_size_)) + (column19_row92);
  const FieldElementT rc_builtin__value6_0 =
      ((rc_builtin__value5_0) * (offset_size_)) + (column19_row108);
  const FieldElementT rc_builtin__value7_0 =
      ((rc_builtin__value6_0) * (offset_size_)) + (column19_row124);
  const FieldElementT ecdsa__signature0__doubling_key__x_squared =
      (column20_row4) * (column20_row4);
  const FieldElementT ecdsa__signature0__exponentiate_generator__bit_0 =
      (column20_row13) - ((column20_row45) + (column20_row45));
  const FieldElementT ecdsa__signature0__exponentiate_generator__bit_neg_0 =
      (FieldElementT::One()) - (ecdsa__signature0__exponentiate_generator__bit_0);
  const FieldElementT ecdsa__signature0__exponentiate_key__bit_0 =
      (column20_row6) - ((column20_row22) + (column20_row22));
  const FieldElementT ecdsa__signature0__exponentiate_key__bit_neg_0 =
      (FieldElementT::One()) - (ecdsa__signature0__exponentiate_key__bit_0);
  const FieldElementT bitwise__sum_var_0_0 =
      (((((((column19_row1) + ((column19_row17) * (FieldElementT::ConstexprFromBigInt(0x2_Z)))) +
           ((column19_row33) * (FieldElementT::ConstexprFromBigInt(0x4_Z)))) +
          ((column19_row49) * (FieldElementT::ConstexprFromBigInt(0x8_Z)))) +
         ((column19_row65) * (FieldElementT::ConstexprFromBigInt(0x10000000000000000_Z)))) +
        ((column19_row81) * (FieldElementT::ConstexprFromBigInt(0x20000000000000000_Z)))) +
       ((column19_row97) * (FieldElementT::ConstexprFromBigInt(0x40000000000000000_Z)))) +
      ((column19_row113) * (FieldElementT::ConstexprFromBigInt(0x80000000000000000_Z)));
  const FieldElementT bitwise__sum_var_8_0 =
      ((((((((column19_row129) *
             (FieldElementT::ConstexprFromBigInt(0x100000000000000000000000000000000_Z))) +
            ((column19_row145) *
             (FieldElementT::ConstexprFromBigInt(0x200000000000000000000000000000000_Z)))) +
           ((column19_row161) *
            (FieldElementT::ConstexprFromBigInt(0x400000000000000000000000000000000_Z)))) +
          ((column19_row177) *
           (FieldElementT::ConstexprFromBigInt(0x800000000000000000000000000000000_Z)))) +
         ((column19_row193) * (FieldElementT::ConstexprFromBigInt(
                                  0x1000000000000000000000000000000000000000000000000_Z)))) +
        ((column19_row209) * (FieldElementT::ConstexprFromBigInt(
                                 0x2000000000000000000000000000000000000000000000000_Z)))) +
       ((column19_row225) * (FieldElementT::ConstexprFromBigInt(
                                0x4000000000000000000000000000000000000000000000000_Z)))) +
      ((column19_row241) *
       (FieldElementT::ConstexprFromBigInt(0x8000000000000000000000000000000000000000000000000_Z)));
  FractionFieldElement<FieldElementT> res(FieldElementT::Zero());
  {
    // Compute a sum of constraints with denominator = domain0.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain4.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/decode/opcode_rc/bit:
        const FieldElementT constraint =
            ((cpu__decode__opcode_rc__bit_0) * (cpu__decode__opcode_rc__bit_0)) -
            (cpu__decode__opcode_rc__bit_0);
        inner_sum += random_coefficients[0] * constraint;
      }
      outer_sum += inner_sum * domain4;
    }

    {
      // Compute a sum of constraints with numerator = domain8.
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
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column2_row0) - (pedersen__points__y))) -
            ((column13_row0) * ((column1_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[63] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column13_row0) * (column13_row0)) -
            ((pedersen__hash0__ec_subset_sum__bit_0) *
             (((column1_row0) + (pedersen__points__x)) + (column1_row1)));
        inner_sum += random_coefficients[64] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column2_row0) + (column2_row1))) -
            ((column13_row0) * ((column1_row0) - (column1_row1)));
        inner_sum += random_coefficients[65] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column1_row1) - (column1_row0));
        inner_sum += random_coefficients[66] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column2_row1) - (column2_row0));
        inner_sum += random_coefficients[67] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/booleanity_test:
        const FieldElementT constraint =
            (pedersen__hash1__ec_subset_sum__bit_0) *
            ((pedersen__hash1__ec_subset_sum__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[78] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/add_points/slope:
        const FieldElementT constraint =
            ((pedersen__hash1__ec_subset_sum__bit_0) * ((column5_row0) - (pedersen__points__y))) -
            ((column14_row0) * ((column4_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[81] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column14_row0) * (column14_row0)) -
            ((pedersen__hash1__ec_subset_sum__bit_0) *
             (((column4_row0) + (pedersen__points__x)) + (column4_row1)));
        inner_sum += random_coefficients[82] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash1__ec_subset_sum__bit_0) * ((column5_row0) + (column5_row1))) -
            ((column14_row0) * ((column4_row0) - (column4_row1)));
        inner_sum += random_coefficients[83] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash1__ec_subset_sum__bit_neg_0) * ((column4_row1) - (column4_row0));
        inner_sum += random_coefficients[84] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash1__ec_subset_sum__bit_neg_0) * ((column5_row1) - (column5_row0));
        inner_sum += random_coefficients[85] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/booleanity_test:
        const FieldElementT constraint =
            (pedersen__hash2__ec_subset_sum__bit_0) *
            ((pedersen__hash2__ec_subset_sum__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[96] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/add_points/slope:
        const FieldElementT constraint =
            ((pedersen__hash2__ec_subset_sum__bit_0) * ((column8_row0) - (pedersen__points__y))) -
            ((column15_row0) * ((column7_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[99] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column15_row0) * (column15_row0)) -
            ((pedersen__hash2__ec_subset_sum__bit_0) *
             (((column7_row0) + (pedersen__points__x)) + (column7_row1)));
        inner_sum += random_coefficients[100] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash2__ec_subset_sum__bit_0) * ((column8_row0) + (column8_row1))) -
            ((column15_row0) * ((column7_row0) - (column7_row1)));
        inner_sum += random_coefficients[101] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash2__ec_subset_sum__bit_neg_0) * ((column7_row1) - (column7_row0));
        inner_sum += random_coefficients[102] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash2__ec_subset_sum__bit_neg_0) * ((column8_row1) - (column8_row0));
        inner_sum += random_coefficients[103] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/booleanity_test:
        const FieldElementT constraint =
            (pedersen__hash3__ec_subset_sum__bit_0) *
            ((pedersen__hash3__ec_subset_sum__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[114] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/add_points/slope:
        const FieldElementT constraint =
            ((pedersen__hash3__ec_subset_sum__bit_0) * ((column11_row0) - (pedersen__points__y))) -
            ((column16_row0) * ((column10_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[117] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column16_row0) * (column16_row0)) -
            ((pedersen__hash3__ec_subset_sum__bit_0) *
             (((column10_row0) + (pedersen__points__x)) + (column10_row1)));
        inner_sum += random_coefficients[118] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash3__ec_subset_sum__bit_0) * ((column11_row0) + (column11_row1))) -
            ((column16_row0) * ((column10_row0) - (column10_row1)));
        inner_sum += random_coefficients[119] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash3__ec_subset_sum__bit_neg_0) * ((column10_row1) - (column10_row0));
        inner_sum += random_coefficients[120] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash3__ec_subset_sum__bit_neg_0) * ((column11_row1) - (column11_row0));
        inner_sum += random_coefficients[121] * constraint;
      }
      outer_sum += inner_sum * domain8;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain0);
  }

  {
    // Compute a sum of constraints with denominator = domain4.
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
    res += FractionFieldElement<FieldElementT>(outer_sum, domain4);
  }

  {
    // Compute a sum of constraints with denominator = domain5.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/decode/opcode_rc_input:
        const FieldElementT constraint =
            (column17_row1) -
            (((((((column0_row0) * (offset_size_)) + (column19_row4)) * (offset_size_)) +
               (column19_row8)) *
              (offset_size_)) +
             (column19_row0));
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
            ((column17_row8) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_0) * (column19_row11)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_0)) * (column19_row3))) +
             (column19_row0));
        inner_sum += random_coefficients[7] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem0_addr:
        const FieldElementT constraint =
            ((column17_row4) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_1) * (column19_row11)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_1)) * (column19_row3))) +
             (column19_row8));
        inner_sum += random_coefficients[8] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem1_addr:
        const FieldElementT constraint = ((column17_row12) + (half_offset_size_)) -
                                         ((((((cpu__decode__opcode_rc__bit_2) * (column17_row0)) +
                                             ((cpu__decode__opcode_rc__bit_4) * (column19_row3))) +
                                            ((cpu__decode__opcode_rc__bit_3) * (column19_row11))) +
                                           ((cpu__decode__flag_op1_base_op0_0) * (column17_row5))) +
                                          (column19_row4));
        inner_sum += random_coefficients[9] * constraint;
      }
      {
        // Constraint expression for cpu/operands/ops_mul:
        const FieldElementT constraint = (column19_row7) - ((column17_row5) * (column17_row13));
        inner_sum += random_coefficients[10] * constraint;
      }
      {
        // Constraint expression for cpu/operands/res:
        const FieldElementT constraint =
            (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column19_row15)) -
            ((((cpu__decode__opcode_rc__bit_5) * ((column17_row5) + (column17_row13))) +
              ((cpu__decode__opcode_rc__bit_6) * (column19_row7))) +
             ((cpu__decode__flag_res_op1_0) * (column17_row13)));
        inner_sum += random_coefficients[11] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_fp:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column17_row9) - (column19_row11));
        inner_sum += random_coefficients[18] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_pc:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) *
            ((column17_row5) -
             (((column17_row0) + (cpu__decode__opcode_rc__bit_2)) + (FieldElementT::One())));
        inner_sum += random_coefficients[19] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off0:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column19_row0) - (half_offset_size_));
        inner_sum += random_coefficients[20] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off1:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) *
            ((column19_row8) - ((half_offset_size_) + (FieldElementT::One())));
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
            (((column19_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z))) - (half_offset_size_));
        inner_sum += random_coefficients[23] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/off2:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_13) *
            (((column19_row4) + (FieldElementT::One())) - (half_offset_size_));
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
            (cpu__decode__opcode_rc__bit_14) * ((column17_row9) - (column19_row15));
        inner_sum += random_coefficients[26] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain22.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp0:
        const FieldElementT constraint =
            (column20_row0) - ((cpu__decode__opcode_rc__bit_9) * (column17_row9));
        inner_sum += random_coefficients[12] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp1:
        const FieldElementT constraint = (column20_row8) - ((column20_row0) * (column19_row15));
        inner_sum += random_coefficients[13] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_negative:
        const FieldElementT constraint =
            ((((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column17_row16)) +
             ((column20_row0) * ((column17_row16) - ((column17_row0) + (column17_row13))))) -
            ((((cpu__decode__flag_pc_update_regular_0) * (npc_reg_0)) +
              ((cpu__decode__opcode_rc__bit_7) * (column19_row15))) +
             ((cpu__decode__opcode_rc__bit_8) * ((column17_row0) + (column19_row15))));
        inner_sum += random_coefficients[14] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_positive:
        const FieldElementT constraint =
            ((column20_row8) - (cpu__decode__opcode_rc__bit_9)) * ((column17_row16) - (npc_reg_0));
        inner_sum += random_coefficients[15] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_ap/ap_update:
        const FieldElementT constraint =
            (column19_row19) -
            ((((column19_row3) + ((cpu__decode__opcode_rc__bit_10) * (column19_row15))) +
              (cpu__decode__opcode_rc__bit_11)) +
             ((cpu__decode__opcode_rc__bit_12) * (FieldElementT::ConstexprFromBigInt(0x2_Z))));
        inner_sum += random_coefficients[16] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_fp/fp_update:
        const FieldElementT constraint =
            (column19_row27) - ((((cpu__decode__fp_update_regular_0) * (column19_row11)) +
                                 ((cpu__decode__opcode_rc__bit_13) * (column17_row9))) +
                                ((cpu__decode__opcode_rc__bit_12) *
                                 ((column19_row3) + (FieldElementT::ConstexprFromBigInt(0x2_Z)))));
        inner_sum += random_coefficients[17] * constraint;
      }
      outer_sum += inner_sum * domain22;
    }

    {
      // Compute a sum of constraints with numerator = domain16.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/doubling_key/slope:
        const FieldElementT constraint = ((((ecdsa__signature0__doubling_key__x_squared) +
                                            (ecdsa__signature0__doubling_key__x_squared)) +
                                           (ecdsa__signature0__doubling_key__x_squared)) +
                                          ((ecdsa__sig_config_).alpha)) -
                                         (((column20_row12) + (column20_row12)) * (column20_row14));
        inner_sum += random_coefficients[145] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/doubling_key/x:
        const FieldElementT constraint = ((column20_row14) * (column20_row14)) -
                                         (((column20_row4) + (column20_row4)) + (column20_row20));
        inner_sum += random_coefficients[146] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/doubling_key/y:
        const FieldElementT constraint = ((column20_row12) + (column20_row28)) -
                                         ((column20_row14) * ((column20_row4) - (column20_row20)));
        inner_sum += random_coefficients[147] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/booleanity_test:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_0) *
            ((ecdsa__signature0__exponentiate_key__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[157] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/slope:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_key__bit_0) * ((column20_row10) - (column20_row12))) -
            ((column20_row1) * ((column20_row2) - (column20_row4)));
        inner_sum += random_coefficients[160] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/x:
        const FieldElementT constraint = ((column20_row1) * (column20_row1)) -
                                         ((ecdsa__signature0__exponentiate_key__bit_0) *
                                          (((column20_row2) + (column20_row4)) + (column20_row18)));
        inner_sum += random_coefficients[161] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/y:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_key__bit_0) * ((column20_row10) + (column20_row26))) -
            ((column20_row1) * ((column20_row2) - (column20_row18)));
        inner_sum += random_coefficients[162] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column20_row9) * ((column20_row2) - (column20_row4))) - (FieldElementT::One());
        inner_sum += random_coefficients[163] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/copy_point/x:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_neg_0) * ((column20_row18) - (column20_row2));
        inner_sum += random_coefficients[164] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/copy_point/y:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_key__bit_neg_0) *
                                         ((column20_row26) - (column20_row10));
        inner_sum += random_coefficients[165] * constraint;
      }
      outer_sum += inner_sum * domain16;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain5);
  }

  {
    // Compute a sum of constraints with denominator = domain23.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for initial_ap:
        const FieldElementT constraint = (column19_row3) - (initial_ap_);
        inner_sum += random_coefficients[27] * constraint;
      }
      {
        // Constraint expression for initial_fp:
        const FieldElementT constraint = (column19_row11) - (initial_ap_);
        inner_sum += random_coefficients[28] * constraint;
      }
      {
        // Constraint expression for initial_pc:
        const FieldElementT constraint = (column17_row0) - (initial_pc_);
        inner_sum += random_coefficients[29] * constraint;
      }
      {
        // Constraint expression for memory/multi_column_perm/perm/init0:
        const FieldElementT constraint =
            (((((memory__multi_column_perm__perm__interaction_elm_) -
                ((column18_row0) +
                 ((memory__multi_column_perm__hash_interaction_elm0_) * (column18_row1)))) *
               (column21_inter1_row0)) +
              (column17_row0)) +
             ((memory__multi_column_perm__hash_interaction_elm0_) * (column17_row1))) -
            (memory__multi_column_perm__perm__interaction_elm_);
        inner_sum += random_coefficients[33] * constraint;
      }
      {
        // Constraint expression for memory/initial_addr:
        const FieldElementT constraint = (column18_row0) - (FieldElementT::One());
        inner_sum += random_coefficients[38] * constraint;
      }
      {
        // Constraint expression for rc16/perm/init0:
        const FieldElementT constraint =
            ((((rc16__perm__interaction_elm_) - (column19_row2)) * (column21_inter1_row1)) +
             (column19_row0)) -
            (rc16__perm__interaction_elm_);
        inner_sum += random_coefficients[41] * constraint;
      }
      {
        // Constraint expression for rc16/minimum:
        const FieldElementT constraint = (column19_row2) - (rc_min_);
        inner_sum += random_coefficients[45] * constraint;
      }
      {
        // Constraint expression for diluted_check/permutation/init0:
        const FieldElementT constraint =
            ((((diluted_check__permutation__interaction_elm_) - (column19_row5)) *
              (column21_inter1_row7)) +
             (column19_row1)) -
            (diluted_check__permutation__interaction_elm_);
        inner_sum += random_coefficients[47] * constraint;
      }
      {
        // Constraint expression for diluted_check/init:
        const FieldElementT constraint = (column21_inter1_row3) - (FieldElementT::One());
        inner_sum += random_coefficients[50] * constraint;
      }
      {
        // Constraint expression for diluted_check/first_element:
        const FieldElementT constraint = (column19_row5) - (diluted_check__first_elm_);
        inner_sum += random_coefficients[51] * constraint;
      }
      {
        // Constraint expression for pedersen/init_addr:
        const FieldElementT constraint = (column17_row6) - (initial_pedersen_addr_);
        inner_sum += random_coefficients[131] * constraint;
      }
      {
        // Constraint expression for rc_builtin/init_addr:
        const FieldElementT constraint = (column17_row102) - (initial_rc_addr_);
        inner_sum += random_coefficients[144] * constraint;
      }
      {
        // Constraint expression for ecdsa/init_addr:
        const FieldElementT constraint = (column17_row22) - (initial_ecdsa_addr_);
        inner_sum += random_coefficients[181] * constraint;
      }
      {
        // Constraint expression for bitwise/init_var_pool_addr:
        const FieldElementT constraint = (column17_row150) - (initial_bitwise_addr_);
        inner_sum += random_coefficients[186] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain23);
  }

  {
    // Compute a sum of constraints with denominator = domain22.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for final_ap:
        const FieldElementT constraint = (column19_row3) - (final_ap_);
        inner_sum += random_coefficients[30] * constraint;
      }
      {
        // Constraint expression for final_fp:
        const FieldElementT constraint = (column19_row11) - (initial_ap_);
        inner_sum += random_coefficients[31] * constraint;
      }
      {
        // Constraint expression for final_pc:
        const FieldElementT constraint = (column17_row0) - (final_pc_);
        inner_sum += random_coefficients[32] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain22);
  }

  {
    // Compute a sum of constraints with denominator = domain1.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain24.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/step0:
        const FieldElementT constraint =
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column18_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column18_row3)))) *
             (column21_inter1_row2)) -
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column17_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column17_row3)))) *
             (column21_inter1_row0));
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
        const FieldElementT constraint = ((memory__address_diff_0) - (FieldElementT::One())) *
                                         ((column18_row1) - (column18_row3));
        inner_sum += random_coefficients[37] * constraint;
      }
      outer_sum += inner_sum * domain24;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain1);
  }

  {
    // Compute a sum of constraints with denominator = domain24.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/last:
        const FieldElementT constraint =
            (column21_inter1_row0) - (memory__multi_column_perm__perm__public_memory_prod_);
        inner_sum += random_coefficients[35] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain24);
  }

  {
    // Compute a sum of constraints with denominator = domain3.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for public_memory_addr_zero:
        const FieldElementT constraint = column17_row2;
        inner_sum += random_coefficients[39] * constraint;
      }
      {
        // Constraint expression for public_memory_value_zero:
        const FieldElementT constraint = column17_row3;
        inner_sum += random_coefficients[40] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain25.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for diluted_check/permutation/step0:
        const FieldElementT constraint =
            (((diluted_check__permutation__interaction_elm_) - (column19_row13)) *
             (column21_inter1_row15)) -
            (((diluted_check__permutation__interaction_elm_) - (column19_row9)) *
             (column21_inter1_row7));
        inner_sum += random_coefficients[48] * constraint;
      }
      {
        // Constraint expression for diluted_check/step:
        const FieldElementT constraint =
            (column21_inter1_row11) -
            (((column21_inter1_row3) *
              ((FieldElementT::One()) +
               ((diluted_check__interaction_z_) * ((column19_row13) - (column19_row5))))) +
             (((diluted_check__interaction_alpha_) * ((column19_row13) - (column19_row5))) *
              ((column19_row13) - (column19_row5))));
        inner_sum += random_coefficients[52] * constraint;
      }
      outer_sum += inner_sum * domain25;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain3);
  }

  {
    // Compute a sum of constraints with denominator = domain2.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain26.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc16/perm/step0:
        const FieldElementT constraint =
            (((rc16__perm__interaction_elm_) - (column19_row6)) * (column21_inter1_row5)) -
            (((rc16__perm__interaction_elm_) - (column19_row4)) * (column21_inter1_row1));
        inner_sum += random_coefficients[42] * constraint;
      }
      {
        // Constraint expression for rc16/diff_is_bit:
        const FieldElementT constraint = ((rc16__diff_0) * (rc16__diff_0)) - (rc16__diff_0);
        inner_sum += random_coefficients[44] * constraint;
      }
      outer_sum += inner_sum * domain26;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain2);
  }

  {
    // Compute a sum of constraints with denominator = domain26.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc16/perm/last:
        const FieldElementT constraint = (column21_inter1_row1) - (rc16__perm__public_memory_prod_);
        inner_sum += random_coefficients[43] * constraint;
      }
      {
        // Constraint expression for rc16/maximum:
        const FieldElementT constraint = (column19_row2) - (rc_max_);
        inner_sum += random_coefficients[46] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain26);
  }

  {
    // Compute a sum of constraints with denominator = domain25.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for diluted_check/permutation/last:
        const FieldElementT constraint =
            (column21_inter1_row7) - (diluted_check__permutation__public_memory_prod_);
        inner_sum += random_coefficients[49] * constraint;
      }
      {
        // Constraint expression for diluted_check/last:
        const FieldElementT constraint = (column21_inter1_row3) - (diluted_check__final_cum_val_);
        inner_sum += random_coefficients[53] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain25);
  }

  {
    // Compute a sum of constraints with denominator = domain9.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column14_row255) * ((column3_row0) - ((column3_row1) + (column3_row1)));
        inner_sum += random_coefficients[54] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column14_row255) *
            ((column3_row1) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column3_row192)));
        inner_sum += random_coefficients[55] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column14_row255) -
            ((column13_row255) * ((column3_row192) - ((column3_row193) + (column3_row193))));
        inner_sum += random_coefficients[56] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column13_row255) *
            ((column3_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column3_row196)));
        inner_sum += random_coefficients[57] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column13_row255) - (((column3_row251) - ((column3_row252) + (column3_row252))) *
                                 ((column3_row196) - ((column3_row197) + (column3_row197))));
        inner_sum += random_coefficients[58] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column3_row251) - ((column3_row252) + (column3_row252))) *
            ((column3_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column3_row251)));
        inner_sum += random_coefficients[59] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column16_row255) * ((column6_row0) - ((column6_row1) + (column6_row1)));
        inner_sum += random_coefficients[72] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash1/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column16_row255) *
            ((column6_row1) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column6_row192)));
        inner_sum += random_coefficients[73] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column16_row255) -
            ((column15_row255) * ((column6_row192) - ((column6_row193) + (column6_row193))));
        inner_sum += random_coefficients[74] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash1/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column15_row255) *
            ((column6_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column6_row196)));
        inner_sum += random_coefficients[75] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column15_row255) - (((column6_row251) - ((column6_row252) + (column6_row252))) *
                                 ((column6_row196) - ((column6_row197) + (column6_row197))));
        inner_sum += random_coefficients[76] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash1/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column6_row251) - ((column6_row252) + (column6_row252))) *
            ((column6_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column6_row251)));
        inner_sum += random_coefficients[77] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column20_row147) * ((column9_row0) - ((column9_row1) + (column9_row1)));
        inner_sum += random_coefficients[90] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash2/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column20_row147) *
            ((column9_row1) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column9_row192)));
        inner_sum += random_coefficients[91] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column20_row147) -
            ((column20_row19) * ((column9_row192) - ((column9_row193) + (column9_row193))));
        inner_sum += random_coefficients[92] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash2/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column20_row19) *
            ((column9_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column9_row196)));
        inner_sum += random_coefficients[93] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column20_row19) - (((column9_row251) - ((column9_row252) + (column9_row252))) *
                                ((column9_row196) - ((column9_row197) + (column9_row197))));
        inner_sum += random_coefficients[94] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash2/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column9_row251) - ((column9_row252) + (column9_row252))) *
            ((column9_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column9_row251)));
        inner_sum += random_coefficients[95] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column20_row211) * ((column12_row0) - ((column12_row1) + (column12_row1)));
        inner_sum += random_coefficients[108] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash3/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column20_row211) *
            ((column12_row1) - ((FieldElementT::ConstexprFromBigInt(
                                    0x800000000000000000000000000000000000000000000000_Z)) *
                                (column12_row192)));
        inner_sum += random_coefficients[109] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column20_row211) -
            ((column20_row83) * ((column12_row192) - ((column12_row193) + (column12_row193))));
        inner_sum += random_coefficients[110] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash3/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column20_row83) *
            ((column12_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column12_row196)));
        inner_sum += random_coefficients[111] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column20_row83) - (((column12_row251) - ((column12_row252) + (column12_row252))) *
                                ((column12_row196) - ((column12_row197) + (column12_row197))));
        inner_sum += random_coefficients[112] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash3/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column12_row251) - ((column12_row252) + (column12_row252))) *
            ((column12_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column12_row251)));
        inner_sum += random_coefficients[113] * constraint;
      }
      {
        // Constraint expression for bitwise/partition:
        const FieldElementT constraint =
            ((bitwise__sum_var_0_0) + (bitwise__sum_var_8_0)) - (column17_row151);
        inner_sum += random_coefficients[190] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain11.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/copy_point/x:
        const FieldElementT constraint = (column1_row256) - (column1_row255);
        inner_sum += random_coefficients[68] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/copy_point/y:
        const FieldElementT constraint = (column2_row256) - (column2_row255);
        inner_sum += random_coefficients[69] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/copy_point/x:
        const FieldElementT constraint = (column4_row256) - (column4_row255);
        inner_sum += random_coefficients[86] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/copy_point/y:
        const FieldElementT constraint = (column5_row256) - (column5_row255);
        inner_sum += random_coefficients[87] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/copy_point/x:
        const FieldElementT constraint = (column7_row256) - (column7_row255);
        inner_sum += random_coefficients[104] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/copy_point/y:
        const FieldElementT constraint = (column8_row256) - (column8_row255);
        inner_sum += random_coefficients[105] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/copy_point/x:
        const FieldElementT constraint = (column10_row256) - (column10_row255);
        inner_sum += random_coefficients[122] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/copy_point/y:
        const FieldElementT constraint = (column11_row256) - (column11_row255);
        inner_sum += random_coefficients[123] * constraint;
      }
      outer_sum += inner_sum * domain11;
    }

    {
      // Compute a sum of constraints with numerator = domain13.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/step_var_pool_addr:
        const FieldElementT constraint =
            (column17_row406) - ((column17_row150) + (FieldElementT::One()));
        inner_sum += random_coefficients[187] * constraint;
      }
      outer_sum += inner_sum * domain13;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain9);
  }

  {
    // Compute a sum of constraints with denominator = domain10.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column3_row0;
        inner_sum += random_coefficients[61] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column6_row0;
        inner_sum += random_coefficients[79] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column9_row0;
        inner_sum += random_coefficients[97] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column12_row0;
        inner_sum += random_coefficients[115] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain10);
  }

  {
    // Compute a sum of constraints with denominator = domain8.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column3_row0;
        inner_sum += random_coefficients[62] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column6_row0;
        inner_sum += random_coefficients[80] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column9_row0;
        inner_sum += random_coefficients[98] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column12_row0;
        inner_sum += random_coefficients[116] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain8);
  }

  {
    // Compute a sum of constraints with denominator = domain12.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/init/x:
        const FieldElementT constraint = (column1_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[70] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/init/y:
        const FieldElementT constraint = (column2_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[71] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/init/x:
        const FieldElementT constraint = (column4_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[88] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/init/y:
        const FieldElementT constraint = (column5_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[89] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/init/x:
        const FieldElementT constraint = (column7_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[106] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/init/y:
        const FieldElementT constraint = (column8_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[107] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/init/x:
        const FieldElementT constraint = (column10_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[124] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/init/y:
        const FieldElementT constraint = (column11_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[125] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value0:
        const FieldElementT constraint = (column17_row7) - (column3_row0);
        inner_sum += random_coefficients[126] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value1:
        const FieldElementT constraint = (column17_row135) - (column6_row0);
        inner_sum += random_coefficients[127] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value2:
        const FieldElementT constraint = (column17_row263) - (column9_row0);
        inner_sum += random_coefficients[128] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value3:
        const FieldElementT constraint = (column17_row391) - (column12_row0);
        inner_sum += random_coefficients[129] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value0:
        const FieldElementT constraint = (column17_row71) - (column3_row256);
        inner_sum += random_coefficients[132] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value1:
        const FieldElementT constraint = (column17_row199) - (column6_row256);
        inner_sum += random_coefficients[133] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value2:
        const FieldElementT constraint = (column17_row327) - (column9_row256);
        inner_sum += random_coefficients[134] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value3:
        const FieldElementT constraint = (column17_row455) - (column12_row256);
        inner_sum += random_coefficients[135] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value0:
        const FieldElementT constraint = (column17_row39) - (column1_row511);
        inner_sum += random_coefficients[137] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value1:
        const FieldElementT constraint = (column17_row167) - (column4_row511);
        inner_sum += random_coefficients[138] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value2:
        const FieldElementT constraint = (column17_row295) - (column7_row511);
        inner_sum += random_coefficients[139] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value3:
        const FieldElementT constraint = (column17_row423) - (column10_row511);
        inner_sum += random_coefficients[140] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain12);
  }

  {
    // Compute a sum of constraints with denominator = domain7.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain27.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input0_addr:
        const FieldElementT constraint =
            (column17_row134) - ((column17_row38) + (FieldElementT::One()));
        inner_sum += random_coefficients[130] * constraint;
      }
      {
        // Constraint expression for rc_builtin/addr_step:
        const FieldElementT constraint =
            (column17_row230) - ((column17_row102) + (FieldElementT::One()));
        inner_sum += random_coefficients[143] * constraint;
      }
      outer_sum += inner_sum * domain27;
    }

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input1_addr:
        const FieldElementT constraint =
            (column17_row70) - ((column17_row6) + (FieldElementT::One()));
        inner_sum += random_coefficients[136] * constraint;
      }
      {
        // Constraint expression for pedersen/output_addr:
        const FieldElementT constraint =
            (column17_row38) - ((column17_row70) + (FieldElementT::One()));
        inner_sum += random_coefficients[141] * constraint;
      }
      {
        // Constraint expression for rc_builtin/value:
        const FieldElementT constraint = (rc_builtin__value7_0) - (column17_row103);
        inner_sum += random_coefficients[142] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain7);
  }

  {
    // Compute a sum of constraints with denominator = domain6.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain19.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/booleanity_test:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_generator__bit_0) *
            ((ecdsa__signature0__exponentiate_generator__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[148] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/slope:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_generator__bit_0) *
             ((column20_row21) - (ecdsa__generator_points__y))) -
            ((column20_row29) * ((column20_row5) - (ecdsa__generator_points__x)));
        inner_sum += random_coefficients[151] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/x:
        const FieldElementT constraint =
            ((column20_row29) * (column20_row29)) -
            ((ecdsa__signature0__exponentiate_generator__bit_0) *
             (((column20_row5) + (ecdsa__generator_points__x)) + (column20_row37)));
        inner_sum += random_coefficients[152] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/y:
        const FieldElementT constraint = ((ecdsa__signature0__exponentiate_generator__bit_0) *
                                          ((column20_row21) + (column20_row53))) -
                                         ((column20_row29) * ((column20_row5) - (column20_row37)));
        inner_sum += random_coefficients[153] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column20_row3) * ((column20_row5) - (ecdsa__generator_points__x))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[154] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/copy_point/x:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_generator__bit_neg_0) *
                                         ((column20_row37) - (column20_row5));
        inner_sum += random_coefficients[155] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/copy_point/y:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_generator__bit_neg_0) *
                                         ((column20_row53) - (column20_row21));
        inner_sum += random_coefficients[156] * constraint;
      }
      outer_sum += inner_sum * domain19;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain6);
  }

  {
    // Compute a sum of constraints with denominator = domain20.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/bit_extraction_end:
        const FieldElementT constraint = column20_row13;
        inner_sum += random_coefficients[149] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain20);
  }

  {
    // Compute a sum of constraints with denominator = domain19.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/zeros_tail:
        const FieldElementT constraint = column20_row13;
        inner_sum += random_coefficients[150] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain19);
  }

  {
    // Compute a sum of constraints with denominator = domain17.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/bit_extraction_end:
        const FieldElementT constraint = column20_row6;
        inner_sum += random_coefficients[158] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain17);
  }

  {
    // Compute a sum of constraints with denominator = domain16.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/zeros_tail:
        const FieldElementT constraint = column20_row6;
        inner_sum += random_coefficients[159] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain16);
  }

  {
    // Compute a sum of constraints with denominator = domain21.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/init_gen/x:
        const FieldElementT constraint = (column20_row5) - (((ecdsa__sig_config_).shift_point).x);
        inner_sum += random_coefficients[166] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/init_gen/y:
        const FieldElementT constraint = (column20_row21) + (((ecdsa__sig_config_).shift_point).y);
        inner_sum += random_coefficients[167] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/slope:
        const FieldElementT constraint =
            (column20_row8181) -
            ((column20_row4090) + ((column20_row8189) * ((column20_row8165) - (column20_row4082))));
        inner_sum += random_coefficients[170] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/x:
        const FieldElementT constraint =
            ((column20_row8189) * (column20_row8189)) -
            (((column20_row8165) + (column20_row4082)) + (column20_row4100));
        inner_sum += random_coefficients[171] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/y:
        const FieldElementT constraint =
            ((column20_row8181) + (column20_row4108)) -
            ((column20_row8189) * ((column20_row8165) - (column20_row4100)));
        inner_sum += random_coefficients[172] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/x_diff_inv:
        const FieldElementT constraint =
            ((column20_row8163) * ((column20_row8165) - (column20_row4082))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[173] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/slope:
        const FieldElementT constraint =
            ((column20_row8186) + (((ecdsa__sig_config_).shift_point).y)) -
            ((column20_row4081) * ((column20_row8178) - (((ecdsa__sig_config_).shift_point).x)));
        inner_sum += random_coefficients[174] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/x:
        const FieldElementT constraint =
            ((column20_row4081) * (column20_row4081)) -
            (((column20_row8178) + (((ecdsa__sig_config_).shift_point).x)) + (column20_row6));
        inner_sum += random_coefficients[175] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/x_diff_inv:
        const FieldElementT constraint =
            ((column20_row8177) * ((column20_row8178) - (((ecdsa__sig_config_).shift_point).x))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[176] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/z_nonzero:
        const FieldElementT constraint =
            ((column20_row13) * (column20_row4089)) - (FieldElementT::One());
        inner_sum += random_coefficients[177] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/q_on_curve/x_squared:
        const FieldElementT constraint = (column20_row8185) - ((column20_row4) * (column20_row4));
        inner_sum += random_coefficients[179] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/q_on_curve/on_curve:
        const FieldElementT constraint = ((column20_row12) * (column20_row12)) -
                                         ((((column20_row4) * (column20_row8185)) +
                                           (((ecdsa__sig_config_).alpha) * (column20_row4))) +
                                          ((ecdsa__sig_config_).beta));
        inner_sum += random_coefficients[180] * constraint;
      }
      {
        // Constraint expression for ecdsa/message_addr:
        const FieldElementT constraint =
            (column17_row4118) - ((column17_row22) + (FieldElementT::One()));
        inner_sum += random_coefficients[182] * constraint;
      }
      {
        // Constraint expression for ecdsa/message_value0:
        const FieldElementT constraint = (column17_row4119) - (column20_row13);
        inner_sum += random_coefficients[184] * constraint;
      }
      {
        // Constraint expression for ecdsa/pubkey_value0:
        const FieldElementT constraint = (column17_row23) - (column20_row4);
        inner_sum += random_coefficients[185] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain28.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/pubkey_addr:
        const FieldElementT constraint =
            (column17_row8214) - ((column17_row4118) + (FieldElementT::One()));
        inner_sum += random_coefficients[183] * constraint;
      }
      outer_sum += inner_sum * domain28;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain21);
  }

  {
    // Compute a sum of constraints with denominator = domain18.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/init_key/x:
        const FieldElementT constraint = (column20_row2) - (((ecdsa__sig_config_).shift_point).x);
        inner_sum += random_coefficients[168] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/init_key/y:
        const FieldElementT constraint = (column20_row10) - (((ecdsa__sig_config_).shift_point).y);
        inner_sum += random_coefficients[169] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/r_and_w_nonzero:
        const FieldElementT constraint =
            ((column20_row6) * (column20_row4094)) - (FieldElementT::One());
        inner_sum += random_coefficients[178] * constraint;
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
        // Constraint expression for bitwise/x_or_y_addr:
        const FieldElementT constraint =
            (column17_row534) - ((column17_row918) + (FieldElementT::One()));
        inner_sum += random_coefficients[188] * constraint;
      }
      {
        // Constraint expression for bitwise/or_is_and_plus_xor:
        const FieldElementT constraint =
            (column17_row535) - ((column17_row663) + (column17_row919));
        inner_sum += random_coefficients[191] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking192:
        const FieldElementT constraint = (((column19_row705) + (column19_row961)) *
                                          (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
                                         (column19_row9);
        inner_sum += random_coefficients[193] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking193:
        const FieldElementT constraint = (((column19_row721) + (column19_row977)) *
                                          (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
                                         (column19_row521);
        inner_sum += random_coefficients[194] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking194:
        const FieldElementT constraint = (((column19_row737) + (column19_row993)) *
                                          (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
                                         (column19_row265);
        inner_sum += random_coefficients[195] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking195:
        const FieldElementT constraint = (((column19_row753) + (column19_row1009)) *
                                          (FieldElementT::ConstexprFromBigInt(0x100_Z))) -
                                         (column19_row777);
        inner_sum += random_coefficients[196] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain29.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/next_var_pool_addr:
        const FieldElementT constraint =
            (column17_row1174) - ((column17_row534) + (FieldElementT::One()));
        inner_sum += random_coefficients[189] * constraint;
      }
      outer_sum += inner_sum * domain29;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain14);
  }

  {
    // Compute a sum of constraints with denominator = domain15.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/addition_is_xor_with_and:
        const FieldElementT constraint =
            ((column19_row1) + (column19_row257)) -
            (((column19_row769) + (column19_row513)) + (column19_row513));
        inner_sum += random_coefficients[192] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain15);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> CpuAirDefinition<FieldElementT, 5>::DomainEvalsAtPoint(
    gsl::span<const FieldElementT> point_powers,
    gsl::span<const FieldElementT> shifts) const {
  const FieldElementT& domain0 = (point_powers[1]) - (FieldElementT::One());
  const FieldElementT& domain1 = (point_powers[2]) - (FieldElementT::One());
  const FieldElementT& domain2 = (point_powers[3]) - (FieldElementT::One());
  const FieldElementT& domain3 = (point_powers[4]) - (FieldElementT::One());
  const FieldElementT& domain4 = (point_powers[5]) - (shifts[0]);
  const FieldElementT& domain5 = (point_powers[5]) - (FieldElementT::One());
  const FieldElementT& domain6 = (point_powers[6]) - (FieldElementT::One());
  const FieldElementT& domain7 = (point_powers[7]) - (FieldElementT::One());
  const FieldElementT& domain8 = (point_powers[8]) - (shifts[1]);
  const FieldElementT& domain9 = (point_powers[8]) - (FieldElementT::One());
  const FieldElementT& domain10 = (point_powers[8]) - (shifts[2]);
  const FieldElementT& domain11 = (point_powers[9]) - (shifts[3]);
  const FieldElementT& domain12 = (point_powers[9]) - (FieldElementT::One());
  const FieldElementT& domain13 = (point_powers[10]) - (shifts[4]);
  const FieldElementT& domain14 = (point_powers[10]) - (FieldElementT::One());
  const FieldElementT& domain15 =
      ((point_powers[10]) - (shifts[5])) * ((point_powers[10]) - (shifts[6])) *
      ((point_powers[10]) - (shifts[7])) * ((point_powers[10]) - (shifts[8])) *
      ((point_powers[10]) - (shifts[9])) * ((point_powers[10]) - (shifts[10])) *
      ((point_powers[10]) - (shifts[11])) * ((point_powers[10]) - (shifts[12])) *
      ((point_powers[10]) - (shifts[13])) * ((point_powers[10]) - (shifts[14])) *
      ((point_powers[10]) - (shifts[15])) * ((point_powers[10]) - (shifts[16])) *
      ((point_powers[10]) - (shifts[17])) * ((point_powers[10]) - (shifts[18])) *
      ((point_powers[10]) - (shifts[19])) * (domain14);
  const FieldElementT& domain16 = (point_powers[11]) - (shifts[1]);
  const FieldElementT& domain17 = (point_powers[11]) - (shifts[20]);
  const FieldElementT& domain18 = (point_powers[11]) - (FieldElementT::One());
  const FieldElementT& domain19 = (point_powers[12]) - (shifts[1]);
  const FieldElementT& domain20 = (point_powers[12]) - (shifts[20]);
  const FieldElementT& domain21 = (point_powers[12]) - (FieldElementT::One());
  return {
      domain0,  domain1,  domain2,  domain3,  domain4,  domain5,  domain6,  domain7,
      domain8,  domain9,  domain10, domain11, domain12, domain13, domain14, domain15,
      domain16, domain17, domain18, domain19, domain20, domain21,
  };
}

template <typename FieldElementT>
TraceGenerationContext CpuAirDefinition<FieldElementT, 5>::GetTraceGenerationContext() const {
  TraceGenerationContext ctx;

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 1024)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 1024)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 1024)) - (1)) <= (SafeDiv(trace_length_, 1024)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 1024)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 1024)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 1024)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 1024)) <= (SafeDiv(trace_length_, 1024)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 1024)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 1024)), "Index out of range.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 8192)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 8192)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 8192)) <= (SafeDiv(trace_length_, 8192)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 8192)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 8192)), "Index out of range.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 8192)) - (1)) <= (SafeDiv(trace_length_, 8192)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 8192)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 8192)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 8192)), "Index out of range.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 128)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 128)), "Index out of range.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 128)), "Index out of range.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 128)) - (1)) <= (SafeDiv(trace_length_, 128)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 128)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 128)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 128)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 128)) <= (SafeDiv(trace_length_, 128)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 128)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 512)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((4) <= (SafeDiv(trace_length_, 128)), "step must not exceed dimension.");

  ASSERT_RELEASE((3) <= (SafeDiv(trace_length_, 128)), "Index out of range.");

  ASSERT_RELEASE((2) <= (SafeDiv(trace_length_, 128)), "Index out of range.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 8)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 8)) + (-1)) < (SafeDiv(trace_length_, 8)), "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 8)) + (-1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 8)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 8)) - (1)) <= (SafeDiv(trace_length_, 8)), "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 8)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 8)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 8)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 8)) <= (SafeDiv(trace_length_, 8)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 8)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 8)), "Index out of range.");

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
      "cpu/decode/opcode_rc/column",
      VirtualColumn(/*column=*/kColumn0Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn2Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn9Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn10Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn11Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn12Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn13Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn14Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn15Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn16Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "mem_pool/addr", VirtualColumn(/*column=*/kColumn17Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "mem_pool/value", VirtualColumn(/*column=*/kColumn17Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "memory/sorted/addr",
      VirtualColumn(/*column=*/kColumn18Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/sorted/value",
      VirtualColumn(/*column=*/kColumn18Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "rc16_pool", VirtualColumn(/*column=*/kColumn19Column, /*step=*/4, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "rc16/sorted", VirtualColumn(/*column=*/kColumn19Column, /*step=*/4, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "diluted_pool", VirtualColumn(/*column=*/kColumn19Column, /*step=*/8, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "diluted_check/permuted_values",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "cpu/registers/ap", VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "cpu/registers/fp",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/11));
  ctx.AddVirtualColumn(
      "cpu/operands/ops_mul",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "cpu/operands/res",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/15));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp0",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp1",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/key_points/x",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/key_points/y",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/partial_sum/x",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/partial_sum/y",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/10));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/selector",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/doubling_slope",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/14));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/slope",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/x_diff_inv",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/partial_sum/x",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/32, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/partial_sum/y",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/32, /*row_offset=*/21));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/selector",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/32, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/slope",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/32, /*row_offset=*/29));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/x_diff_inv",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/32, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn13Column, /*step=*/256, /*row_offset=*/255));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn14Column, /*step=*/256, /*row_offset=*/255));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn15Column, /*step=*/256, /*row_offset=*/255));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn16Column, /*step=*/256, /*row_offset=*/255));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/256, /*row_offset=*/19));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/256, /*row_offset=*/147));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/256, /*row_offset=*/83));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/256, /*row_offset=*/211));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/r_w_inv",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/4096, /*row_offset=*/4094));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/add_results_slope",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/8192, /*row_offset=*/8189));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/add_results_inv",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/8192, /*row_offset=*/8163));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/extract_r_slope",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/8192, /*row_offset=*/4081));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/extract_r_inv",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/8192, /*row_offset=*/8177));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/z_inv",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/8192, /*row_offset=*/4089));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/q_x_squared",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/8192, /*row_offset=*/8185));
  ctx.AddVirtualColumn(
      "memory/multi_column_perm/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn21Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "rc16/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn21Inter1Column - kNumColumnsFirst, /*step=*/4, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "diluted_check/cumulative_value",
      VirtualColumn(
          /*column=*/kColumn21Inter1Column - kNumColumnsFirst, /*step=*/8, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "diluted_check/permutation/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn21Inter1Column - kNumColumnsFirst, /*step=*/8, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "cpu/decode/mem_inst/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/mem_inst/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/pc", VirtualColumn(/*column=*/kColumn17Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/instruction",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/off0", VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/off1", VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/decode/off2", VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_dst/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_dst/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op0/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op0/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/16, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op1/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op1/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/16, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "orig/public_memory/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/8, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "orig/public_memory/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/8, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "pedersen/input0/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/128, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "pedersen/input0/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/128, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "pedersen/input1/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/128, /*row_offset=*/70));
  ctx.AddVirtualColumn(
      "pedersen/input1/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/128, /*row_offset=*/71));
  ctx.AddVirtualColumn(
      "pedersen/output/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/128, /*row_offset=*/38));
  ctx.AddVirtualColumn(
      "pedersen/output/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/128, /*row_offset=*/39));
  ctx.AddVirtualColumn(
      "rc_builtin/mem/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/128, /*row_offset=*/102));
  ctx.AddVirtualColumn(
      "rc_builtin/mem/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/128, /*row_offset=*/103));
  ctx.AddVirtualColumn(
      "rc_builtin/inner_rc",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "ecdsa/pubkey/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/8192, /*row_offset=*/22));
  ctx.AddVirtualColumn(
      "ecdsa/pubkey/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/8192, /*row_offset=*/23));
  ctx.AddVirtualColumn(
      "ecdsa/message/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/8192, /*row_offset=*/4118));
  ctx.AddVirtualColumn(
      "ecdsa/message/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/8192, /*row_offset=*/4119));
  ctx.AddVirtualColumn(
      "bitwise/x/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1024, /*row_offset=*/150));
  ctx.AddVirtualColumn(
      "bitwise/x/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1024, /*row_offset=*/151));
  ctx.AddVirtualColumn(
      "bitwise/y/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1024, /*row_offset=*/406));
  ctx.AddVirtualColumn(
      "bitwise/y/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1024, /*row_offset=*/407));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1024, /*row_offset=*/662));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1024, /*row_offset=*/663));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1024, /*row_offset=*/918));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1024, /*row_offset=*/919));
  ctx.AddVirtualColumn(
      "bitwise/x_or_y/addr",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1024, /*row_offset=*/534));
  ctx.AddVirtualColumn(
      "bitwise/x_or_y/value",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1024, /*row_offset=*/535));
  ctx.AddVirtualColumn(
      "bitwise/diluted_var_pool",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "bitwise/x", VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "bitwise/y", VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/257));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/513));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/769));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking192",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/1024, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking193",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/1024, /*row_offset=*/521));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking194",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/1024, /*row_offset=*/265));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking195",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/1024, /*row_offset=*/777));

  ctx.AddPeriodicColumn(
      "pedersen/points/x",
      VirtualColumn(/*column=*/kPedersenPointsXPeriodicColumn, /*step=*/1, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "pedersen/points/y",
      VirtualColumn(/*column=*/kPedersenPointsYPeriodicColumn, /*step=*/1, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "ecdsa/generator_points/x",
      VirtualColumn(
          /*column=*/kEcdsaGeneratorPointsXPeriodicColumn, /*step=*/32, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "ecdsa/generator_points/y",
      VirtualColumn(
          /*column=*/kEcdsaGeneratorPointsYPeriodicColumn, /*step=*/32, /*row_offset=*/0));

  ctx.AddObject<std::vector<size_t>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);
  ctx.AddObject<std::vector<size_t>>(
      "pedersen/hash1/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "pedersen/hash1/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);
  ctx.AddObject<std::vector<size_t>>(
      "pedersen/hash2/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "pedersen/hash2/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);
  ctx.AddObject<std::vector<size_t>>(
      "pedersen/hash3/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "pedersen/hash3/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);

  return ctx;
}

template <typename FieldElementT>
std::vector<std::pair<int64_t, uint64_t>> CpuAirDefinition<FieldElementT, 5>::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;

  mask.reserve(246);
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
  mask.emplace_back(255, kColumn1Column);
  mask.emplace_back(256, kColumn1Column);
  mask.emplace_back(511, kColumn1Column);
  mask.emplace_back(0, kColumn2Column);
  mask.emplace_back(1, kColumn2Column);
  mask.emplace_back(255, kColumn2Column);
  mask.emplace_back(256, kColumn2Column);
  mask.emplace_back(0, kColumn3Column);
  mask.emplace_back(1, kColumn3Column);
  mask.emplace_back(192, kColumn3Column);
  mask.emplace_back(193, kColumn3Column);
  mask.emplace_back(196, kColumn3Column);
  mask.emplace_back(197, kColumn3Column);
  mask.emplace_back(251, kColumn3Column);
  mask.emplace_back(252, kColumn3Column);
  mask.emplace_back(256, kColumn3Column);
  mask.emplace_back(0, kColumn4Column);
  mask.emplace_back(1, kColumn4Column);
  mask.emplace_back(255, kColumn4Column);
  mask.emplace_back(256, kColumn4Column);
  mask.emplace_back(511, kColumn4Column);
  mask.emplace_back(0, kColumn5Column);
  mask.emplace_back(1, kColumn5Column);
  mask.emplace_back(255, kColumn5Column);
  mask.emplace_back(256, kColumn5Column);
  mask.emplace_back(0, kColumn6Column);
  mask.emplace_back(1, kColumn6Column);
  mask.emplace_back(192, kColumn6Column);
  mask.emplace_back(193, kColumn6Column);
  mask.emplace_back(196, kColumn6Column);
  mask.emplace_back(197, kColumn6Column);
  mask.emplace_back(251, kColumn6Column);
  mask.emplace_back(252, kColumn6Column);
  mask.emplace_back(256, kColumn6Column);
  mask.emplace_back(0, kColumn7Column);
  mask.emplace_back(1, kColumn7Column);
  mask.emplace_back(255, kColumn7Column);
  mask.emplace_back(256, kColumn7Column);
  mask.emplace_back(511, kColumn7Column);
  mask.emplace_back(0, kColumn8Column);
  mask.emplace_back(1, kColumn8Column);
  mask.emplace_back(255, kColumn8Column);
  mask.emplace_back(256, kColumn8Column);
  mask.emplace_back(0, kColumn9Column);
  mask.emplace_back(1, kColumn9Column);
  mask.emplace_back(192, kColumn9Column);
  mask.emplace_back(193, kColumn9Column);
  mask.emplace_back(196, kColumn9Column);
  mask.emplace_back(197, kColumn9Column);
  mask.emplace_back(251, kColumn9Column);
  mask.emplace_back(252, kColumn9Column);
  mask.emplace_back(256, kColumn9Column);
  mask.emplace_back(0, kColumn10Column);
  mask.emplace_back(1, kColumn10Column);
  mask.emplace_back(255, kColumn10Column);
  mask.emplace_back(256, kColumn10Column);
  mask.emplace_back(511, kColumn10Column);
  mask.emplace_back(0, kColumn11Column);
  mask.emplace_back(1, kColumn11Column);
  mask.emplace_back(255, kColumn11Column);
  mask.emplace_back(256, kColumn11Column);
  mask.emplace_back(0, kColumn12Column);
  mask.emplace_back(1, kColumn12Column);
  mask.emplace_back(192, kColumn12Column);
  mask.emplace_back(193, kColumn12Column);
  mask.emplace_back(196, kColumn12Column);
  mask.emplace_back(197, kColumn12Column);
  mask.emplace_back(251, kColumn12Column);
  mask.emplace_back(252, kColumn12Column);
  mask.emplace_back(256, kColumn12Column);
  mask.emplace_back(0, kColumn13Column);
  mask.emplace_back(255, kColumn13Column);
  mask.emplace_back(0, kColumn14Column);
  mask.emplace_back(255, kColumn14Column);
  mask.emplace_back(0, kColumn15Column);
  mask.emplace_back(255, kColumn15Column);
  mask.emplace_back(0, kColumn16Column);
  mask.emplace_back(255, kColumn16Column);
  mask.emplace_back(0, kColumn17Column);
  mask.emplace_back(1, kColumn17Column);
  mask.emplace_back(2, kColumn17Column);
  mask.emplace_back(3, kColumn17Column);
  mask.emplace_back(4, kColumn17Column);
  mask.emplace_back(5, kColumn17Column);
  mask.emplace_back(6, kColumn17Column);
  mask.emplace_back(7, kColumn17Column);
  mask.emplace_back(8, kColumn17Column);
  mask.emplace_back(9, kColumn17Column);
  mask.emplace_back(12, kColumn17Column);
  mask.emplace_back(13, kColumn17Column);
  mask.emplace_back(16, kColumn17Column);
  mask.emplace_back(22, kColumn17Column);
  mask.emplace_back(23, kColumn17Column);
  mask.emplace_back(38, kColumn17Column);
  mask.emplace_back(39, kColumn17Column);
  mask.emplace_back(70, kColumn17Column);
  mask.emplace_back(71, kColumn17Column);
  mask.emplace_back(102, kColumn17Column);
  mask.emplace_back(103, kColumn17Column);
  mask.emplace_back(134, kColumn17Column);
  mask.emplace_back(135, kColumn17Column);
  mask.emplace_back(150, kColumn17Column);
  mask.emplace_back(151, kColumn17Column);
  mask.emplace_back(167, kColumn17Column);
  mask.emplace_back(199, kColumn17Column);
  mask.emplace_back(230, kColumn17Column);
  mask.emplace_back(263, kColumn17Column);
  mask.emplace_back(295, kColumn17Column);
  mask.emplace_back(327, kColumn17Column);
  mask.emplace_back(391, kColumn17Column);
  mask.emplace_back(406, kColumn17Column);
  mask.emplace_back(423, kColumn17Column);
  mask.emplace_back(455, kColumn17Column);
  mask.emplace_back(534, kColumn17Column);
  mask.emplace_back(535, kColumn17Column);
  mask.emplace_back(663, kColumn17Column);
  mask.emplace_back(918, kColumn17Column);
  mask.emplace_back(919, kColumn17Column);
  mask.emplace_back(1174, kColumn17Column);
  mask.emplace_back(4118, kColumn17Column);
  mask.emplace_back(4119, kColumn17Column);
  mask.emplace_back(8214, kColumn17Column);
  mask.emplace_back(0, kColumn18Column);
  mask.emplace_back(1, kColumn18Column);
  mask.emplace_back(2, kColumn18Column);
  mask.emplace_back(3, kColumn18Column);
  mask.emplace_back(0, kColumn19Column);
  mask.emplace_back(1, kColumn19Column);
  mask.emplace_back(2, kColumn19Column);
  mask.emplace_back(3, kColumn19Column);
  mask.emplace_back(4, kColumn19Column);
  mask.emplace_back(5, kColumn19Column);
  mask.emplace_back(6, kColumn19Column);
  mask.emplace_back(7, kColumn19Column);
  mask.emplace_back(8, kColumn19Column);
  mask.emplace_back(9, kColumn19Column);
  mask.emplace_back(11, kColumn19Column);
  mask.emplace_back(12, kColumn19Column);
  mask.emplace_back(13, kColumn19Column);
  mask.emplace_back(15, kColumn19Column);
  mask.emplace_back(17, kColumn19Column);
  mask.emplace_back(19, kColumn19Column);
  mask.emplace_back(27, kColumn19Column);
  mask.emplace_back(28, kColumn19Column);
  mask.emplace_back(33, kColumn19Column);
  mask.emplace_back(44, kColumn19Column);
  mask.emplace_back(49, kColumn19Column);
  mask.emplace_back(60, kColumn19Column);
  mask.emplace_back(65, kColumn19Column);
  mask.emplace_back(76, kColumn19Column);
  mask.emplace_back(81, kColumn19Column);
  mask.emplace_back(92, kColumn19Column);
  mask.emplace_back(97, kColumn19Column);
  mask.emplace_back(108, kColumn19Column);
  mask.emplace_back(113, kColumn19Column);
  mask.emplace_back(124, kColumn19Column);
  mask.emplace_back(129, kColumn19Column);
  mask.emplace_back(145, kColumn19Column);
  mask.emplace_back(161, kColumn19Column);
  mask.emplace_back(177, kColumn19Column);
  mask.emplace_back(193, kColumn19Column);
  mask.emplace_back(209, kColumn19Column);
  mask.emplace_back(225, kColumn19Column);
  mask.emplace_back(241, kColumn19Column);
  mask.emplace_back(257, kColumn19Column);
  mask.emplace_back(265, kColumn19Column);
  mask.emplace_back(513, kColumn19Column);
  mask.emplace_back(521, kColumn19Column);
  mask.emplace_back(705, kColumn19Column);
  mask.emplace_back(721, kColumn19Column);
  mask.emplace_back(737, kColumn19Column);
  mask.emplace_back(753, kColumn19Column);
  mask.emplace_back(769, kColumn19Column);
  mask.emplace_back(777, kColumn19Column);
  mask.emplace_back(961, kColumn19Column);
  mask.emplace_back(977, kColumn19Column);
  mask.emplace_back(993, kColumn19Column);
  mask.emplace_back(1009, kColumn19Column);
  mask.emplace_back(0, kColumn20Column);
  mask.emplace_back(1, kColumn20Column);
  mask.emplace_back(2, kColumn20Column);
  mask.emplace_back(3, kColumn20Column);
  mask.emplace_back(4, kColumn20Column);
  mask.emplace_back(5, kColumn20Column);
  mask.emplace_back(6, kColumn20Column);
  mask.emplace_back(8, kColumn20Column);
  mask.emplace_back(9, kColumn20Column);
  mask.emplace_back(10, kColumn20Column);
  mask.emplace_back(12, kColumn20Column);
  mask.emplace_back(13, kColumn20Column);
  mask.emplace_back(14, kColumn20Column);
  mask.emplace_back(18, kColumn20Column);
  mask.emplace_back(19, kColumn20Column);
  mask.emplace_back(20, kColumn20Column);
  mask.emplace_back(21, kColumn20Column);
  mask.emplace_back(22, kColumn20Column);
  mask.emplace_back(26, kColumn20Column);
  mask.emplace_back(28, kColumn20Column);
  mask.emplace_back(29, kColumn20Column);
  mask.emplace_back(37, kColumn20Column);
  mask.emplace_back(45, kColumn20Column);
  mask.emplace_back(53, kColumn20Column);
  mask.emplace_back(83, kColumn20Column);
  mask.emplace_back(147, kColumn20Column);
  mask.emplace_back(211, kColumn20Column);
  mask.emplace_back(4081, kColumn20Column);
  mask.emplace_back(4082, kColumn20Column);
  mask.emplace_back(4089, kColumn20Column);
  mask.emplace_back(4090, kColumn20Column);
  mask.emplace_back(4094, kColumn20Column);
  mask.emplace_back(4100, kColumn20Column);
  mask.emplace_back(4108, kColumn20Column);
  mask.emplace_back(8163, kColumn20Column);
  mask.emplace_back(8165, kColumn20Column);
  mask.emplace_back(8177, kColumn20Column);
  mask.emplace_back(8178, kColumn20Column);
  mask.emplace_back(8181, kColumn20Column);
  mask.emplace_back(8185, kColumn20Column);
  mask.emplace_back(8186, kColumn20Column);
  mask.emplace_back(8189, kColumn20Column);
  mask.emplace_back(0, kColumn21Inter1Column);
  mask.emplace_back(1, kColumn21Inter1Column);
  mask.emplace_back(2, kColumn21Inter1Column);
  mask.emplace_back(3, kColumn21Inter1Column);
  mask.emplace_back(5, kColumn21Inter1Column);
  mask.emplace_back(7, kColumn21Inter1Column);
  mask.emplace_back(11, kColumn21Inter1Column);
  mask.emplace_back(15, kColumn21Inter1Column);

  return mask;
}

}  // namespace cpu

}  // namespace starkware
