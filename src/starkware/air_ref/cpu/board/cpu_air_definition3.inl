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
CpuAirDefinition<FieldElementT, 3>::CreateCompositionPolynomial(
    const FieldElement& trace_generator, const ConstFieldElementSpan& random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const FieldElementT& gen = trace_generator.As<FieldElementT>();

  const std::vector<uint64_t> point_exponents = {
      trace_length_,
      SafeDiv(trace_length_, 2),
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
      SafeDiv((251) * (trace_length_), 256),
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
      (trace_length_) - (1),
      (16) * ((SafeDiv(trace_length_, 16)) - (1)),
      (2) * ((SafeDiv(trace_length_, 2)) - (1)),
      (128) * ((SafeDiv(trace_length_, 128)) - (1)),
      (8192) * ((SafeDiv(trace_length_, 8192)) - (1)),
      (4096) * ((SafeDiv(trace_length_, 4096)) - (1))};

  BuildPeriodicColumns(gen, &builder);

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
CpuAirDefinition<FieldElementT, 3>::PrecomputeDomainEvalsOnCoset(
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
      FieldElementT::UninitializedVector(16),   FieldElementT::UninitializedVector(16),
      FieldElementT::UninitializedVector(32),   FieldElementT::UninitializedVector(128),
      FieldElementT::UninitializedVector(256),  FieldElementT::UninitializedVector(256),
      FieldElementT::UninitializedVector(256),  FieldElementT::UninitializedVector(512),
      FieldElementT::UninitializedVector(512),  FieldElementT::UninitializedVector(1024),
      FieldElementT::UninitializedVector(4096), FieldElementT::UninitializedVector(4096),
      FieldElementT::UninitializedVector(4096), FieldElementT::UninitializedVector(4096),
      FieldElementT::UninitializedVector(4096), FieldElementT::UninitializedVector(4096),
      FieldElementT::UninitializedVector(8192), FieldElementT::UninitializedVector(8192),
      FieldElementT::UninitializedVector(8192),
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

  period = 16;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[2][i] = (point_powers[2][i & (15)]) - (shifts[0]);
        }
      },
      period, kTaskSize);

  period = 16;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[3][i] = (point_powers[2][i & (15)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 32;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[4][i] = (point_powers[3][i & (31)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[5][i] = (point_powers[4][i & (127)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[6][i] = (point_powers[5][i & (255)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[7][i] = (point_powers[5][i & (255)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[8][i] = (point_powers[5][i & (255)]) - (shifts[2]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[9][i] = (point_powers[6][i & (511)]) - (shifts[3]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[10][i] = (point_powers[6][i & (511)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[11][i] = (point_powers[7][i & (1023)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[12][i] = (point_powers[8][i & (4095)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[13][i] = (point_powers[8][i & (4095)]) - (shifts[4]);
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[14][i] = (point_powers[8][i & (4095)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[15][i] = (point_powers[8][i & (4095)]) - (shifts[5]);
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[16][i] = ((point_powers[8][i & (4095)]) - (shifts[6])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[7])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[8])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[9])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[10])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[11])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[12])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[13])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[14])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[15])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[16])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[17])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[18])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[19])) *
                                   ((point_powers[8][i & (4095)]) - (shifts[20])) *
                                   (precomp_domains[14][i & (4096 - 1)]);
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[17][i] = (point_powers[8][i & (4095)]) - (shifts[2]);
        }
      },
      period, kTaskSize);

  period = 8192;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[18][i] = (point_powers[9][i & (8191)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 8192;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[19][i] = (point_powers[9][i & (8191)]) - (shifts[4]);
        }
      },
      period, kTaskSize);

  period = 8192;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[20][i] = (point_powers[9][i & (8191)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);
  return precomp_domains;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> CpuAirDefinition<FieldElementT, 3>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 27, "shifts should contain 27 elements.");

  // domain0 = point^trace_length - 1.
  const FieldElementT& domain0 = precomp_domains[0];
  // domain1 = point^(trace_length / 2) - 1.
  const FieldElementT& domain1 = precomp_domains[1];
  // domain2 = point^(trace_length / 16) - gen^(15 * trace_length / 16).
  const FieldElementT& domain2 = precomp_domains[2];
  // domain3 = point^(trace_length / 16) - 1.
  const FieldElementT& domain3 = precomp_domains[3];
  // domain4 = point^(trace_length / 32) - 1.
  const FieldElementT& domain4 = precomp_domains[4];
  // domain5 = point^(trace_length / 128) - 1.
  const FieldElementT& domain5 = precomp_domains[5];
  // domain6 = point^(trace_length / 256) - gen^(255 * trace_length / 256).
  const FieldElementT& domain6 = precomp_domains[6];
  // domain7 = point^(trace_length / 256) - 1.
  const FieldElementT& domain7 = precomp_domains[7];
  // domain8 = point^(trace_length / 256) - gen^(63 * trace_length / 64).
  const FieldElementT& domain8 = precomp_domains[8];
  // domain9 = point^(trace_length / 512) - gen^(trace_length / 2).
  const FieldElementT& domain9 = precomp_domains[9];
  // domain10 = point^(trace_length / 512) - 1.
  const FieldElementT& domain10 = precomp_domains[10];
  // domain11 = point^(trace_length / 1024) - 1.
  const FieldElementT& domain11 = precomp_domains[11];
  // domain12 = point^(trace_length / 4096) - gen^(255 * trace_length / 256).
  const FieldElementT& domain12 = precomp_domains[12];
  // domain13 = point^(trace_length / 4096) - gen^(251 * trace_length / 256).
  const FieldElementT& domain13 = precomp_domains[13];
  // domain14 = point^(trace_length / 4096) - 1.
  const FieldElementT& domain14 = precomp_domains[14];
  // domain15 = point^(trace_length / 4096) - gen^(3 * trace_length / 4).
  const FieldElementT& domain15 = precomp_domains[15];
  // domain16 = (point^(trace_length / 4096) - gen^(trace_length / 64)) * (point^(trace_length /
  // 4096) - gen^(trace_length / 32)) * (point^(trace_length / 4096) - gen^(3 * trace_length / 64))
  // * (point^(trace_length / 4096) - gen^(trace_length / 16)) * (point^(trace_length / 4096) -
  // gen^(5 * trace_length / 64)) * (point^(trace_length / 4096) - gen^(3 * trace_length / 32)) *
  // (point^(trace_length / 4096) - gen^(7 * trace_length / 64)) * (point^(trace_length / 4096) -
  // gen^(trace_length / 8)) * (point^(trace_length / 4096) - gen^(9 * trace_length / 64)) *
  // (point^(trace_length / 4096) - gen^(5 * trace_length / 32)) * (point^(trace_length / 4096) -
  // gen^(11 * trace_length / 64)) * (point^(trace_length / 4096) - gen^(3 * trace_length / 16)) *
  // (point^(trace_length / 4096) - gen^(13 * trace_length / 64)) * (point^(trace_length / 4096) -
  // gen^(7 * trace_length / 32)) * (point^(trace_length / 4096) - gen^(15 * trace_length / 64)) *
  // domain14.
  const FieldElementT& domain16 = precomp_domains[16];
  // domain17 = point^(trace_length / 4096) - gen^(63 * trace_length / 64).
  const FieldElementT& domain17 = precomp_domains[17];
  // domain18 = point^(trace_length / 8192) - gen^(255 * trace_length / 256).
  const FieldElementT& domain18 = precomp_domains[18];
  // domain19 = point^(trace_length / 8192) - gen^(251 * trace_length / 256).
  const FieldElementT& domain19 = precomp_domains[19];
  // domain20 = point^(trace_length / 8192) - 1.
  const FieldElementT& domain20 = precomp_domains[20];
  // domain21 = point - gen^(trace_length - 1).
  const FieldElementT& domain21 = (point) - (shifts[21]);
  // domain22 = point - gen^(16 * (trace_length / 16 - 1)).
  const FieldElementT& domain22 = (point) - (shifts[22]);
  // domain23 = point - 1.
  const FieldElementT& domain23 = (point) - (FieldElementT::One());
  // domain24 = point - gen^(2 * (trace_length / 2 - 1)).
  const FieldElementT& domain24 = (point) - (shifts[23]);
  // domain25 = point - gen^(128 * (trace_length / 128 - 1)).
  const FieldElementT& domain25 = (point) - (shifts[24]);
  // domain26 = point - gen^(8192 * (trace_length / 8192 - 1)).
  const FieldElementT& domain26 = (point) - (shifts[25]);
  // domain27 = point - gen^(4096 * (trace_length / 4096 - 1)).
  const FieldElementT& domain27 = (point) - (shifts[26]);

  ASSERT_VERIFIER(neighbors.size() == 286, "Neighbors must contain 286 elements.");
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
  const FieldElementT& column1_row32 = neighbors[kColumn1Row32Neighbor];
  const FieldElementT& column1_row64 = neighbors[kColumn1Row64Neighbor];
  const FieldElementT& column1_row128 = neighbors[kColumn1Row128Neighbor];
  const FieldElementT& column1_row192 = neighbors[kColumn1Row192Neighbor];
  const FieldElementT& column1_row256 = neighbors[kColumn1Row256Neighbor];
  const FieldElementT& column1_row320 = neighbors[kColumn1Row320Neighbor];
  const FieldElementT& column1_row384 = neighbors[kColumn1Row384Neighbor];
  const FieldElementT& column1_row448 = neighbors[kColumn1Row448Neighbor];
  const FieldElementT& column1_row512 = neighbors[kColumn1Row512Neighbor];
  const FieldElementT& column1_row576 = neighbors[kColumn1Row576Neighbor];
  const FieldElementT& column1_row640 = neighbors[kColumn1Row640Neighbor];
  const FieldElementT& column1_row704 = neighbors[kColumn1Row704Neighbor];
  const FieldElementT& column1_row768 = neighbors[kColumn1Row768Neighbor];
  const FieldElementT& column1_row832 = neighbors[kColumn1Row832Neighbor];
  const FieldElementT& column1_row896 = neighbors[kColumn1Row896Neighbor];
  const FieldElementT& column1_row960 = neighbors[kColumn1Row960Neighbor];
  const FieldElementT& column1_row1024 = neighbors[kColumn1Row1024Neighbor];
  const FieldElementT& column1_row1056 = neighbors[kColumn1Row1056Neighbor];
  const FieldElementT& column1_row2048 = neighbors[kColumn1Row2048Neighbor];
  const FieldElementT& column1_row2080 = neighbors[kColumn1Row2080Neighbor];
  const FieldElementT& column1_row2816 = neighbors[kColumn1Row2816Neighbor];
  const FieldElementT& column1_row2880 = neighbors[kColumn1Row2880Neighbor];
  const FieldElementT& column1_row2944 = neighbors[kColumn1Row2944Neighbor];
  const FieldElementT& column1_row3008 = neighbors[kColumn1Row3008Neighbor];
  const FieldElementT& column1_row3072 = neighbors[kColumn1Row3072Neighbor];
  const FieldElementT& column1_row3104 = neighbors[kColumn1Row3104Neighbor];
  const FieldElementT& column1_row3840 = neighbors[kColumn1Row3840Neighbor];
  const FieldElementT& column1_row3904 = neighbors[kColumn1Row3904Neighbor];
  const FieldElementT& column1_row3968 = neighbors[kColumn1Row3968Neighbor];
  const FieldElementT& column1_row4032 = neighbors[kColumn1Row4032Neighbor];
  const FieldElementT& column2_row0 = neighbors[kColumn2Row0Neighbor];
  const FieldElementT& column2_row1 = neighbors[kColumn2Row1Neighbor];
  const FieldElementT& column3_row0 = neighbors[kColumn3Row0Neighbor];
  const FieldElementT& column3_row1 = neighbors[kColumn3Row1Neighbor];
  const FieldElementT& column3_row255 = neighbors[kColumn3Row255Neighbor];
  const FieldElementT& column3_row256 = neighbors[kColumn3Row256Neighbor];
  const FieldElementT& column3_row511 = neighbors[kColumn3Row511Neighbor];
  const FieldElementT& column4_row0 = neighbors[kColumn4Row0Neighbor];
  const FieldElementT& column4_row1 = neighbors[kColumn4Row1Neighbor];
  const FieldElementT& column4_row255 = neighbors[kColumn4Row255Neighbor];
  const FieldElementT& column4_row256 = neighbors[kColumn4Row256Neighbor];
  const FieldElementT& column5_row0 = neighbors[kColumn5Row0Neighbor];
  const FieldElementT& column5_row1 = neighbors[kColumn5Row1Neighbor];
  const FieldElementT& column5_row192 = neighbors[kColumn5Row192Neighbor];
  const FieldElementT& column5_row193 = neighbors[kColumn5Row193Neighbor];
  const FieldElementT& column5_row196 = neighbors[kColumn5Row196Neighbor];
  const FieldElementT& column5_row197 = neighbors[kColumn5Row197Neighbor];
  const FieldElementT& column5_row251 = neighbors[kColumn5Row251Neighbor];
  const FieldElementT& column5_row252 = neighbors[kColumn5Row252Neighbor];
  const FieldElementT& column5_row256 = neighbors[kColumn5Row256Neighbor];
  const FieldElementT& column6_row0 = neighbors[kColumn6Row0Neighbor];
  const FieldElementT& column6_row1 = neighbors[kColumn6Row1Neighbor];
  const FieldElementT& column6_row255 = neighbors[kColumn6Row255Neighbor];
  const FieldElementT& column6_row256 = neighbors[kColumn6Row256Neighbor];
  const FieldElementT& column6_row511 = neighbors[kColumn6Row511Neighbor];
  const FieldElementT& column7_row0 = neighbors[kColumn7Row0Neighbor];
  const FieldElementT& column7_row1 = neighbors[kColumn7Row1Neighbor];
  const FieldElementT& column7_row255 = neighbors[kColumn7Row255Neighbor];
  const FieldElementT& column7_row256 = neighbors[kColumn7Row256Neighbor];
  const FieldElementT& column8_row0 = neighbors[kColumn8Row0Neighbor];
  const FieldElementT& column8_row1 = neighbors[kColumn8Row1Neighbor];
  const FieldElementT& column8_row192 = neighbors[kColumn8Row192Neighbor];
  const FieldElementT& column8_row193 = neighbors[kColumn8Row193Neighbor];
  const FieldElementT& column8_row196 = neighbors[kColumn8Row196Neighbor];
  const FieldElementT& column8_row197 = neighbors[kColumn8Row197Neighbor];
  const FieldElementT& column8_row251 = neighbors[kColumn8Row251Neighbor];
  const FieldElementT& column8_row252 = neighbors[kColumn8Row252Neighbor];
  const FieldElementT& column8_row256 = neighbors[kColumn8Row256Neighbor];
  const FieldElementT& column9_row0 = neighbors[kColumn9Row0Neighbor];
  const FieldElementT& column9_row1 = neighbors[kColumn9Row1Neighbor];
  const FieldElementT& column9_row255 = neighbors[kColumn9Row255Neighbor];
  const FieldElementT& column9_row256 = neighbors[kColumn9Row256Neighbor];
  const FieldElementT& column9_row511 = neighbors[kColumn9Row511Neighbor];
  const FieldElementT& column10_row0 = neighbors[kColumn10Row0Neighbor];
  const FieldElementT& column10_row1 = neighbors[kColumn10Row1Neighbor];
  const FieldElementT& column10_row255 = neighbors[kColumn10Row255Neighbor];
  const FieldElementT& column10_row256 = neighbors[kColumn10Row256Neighbor];
  const FieldElementT& column11_row0 = neighbors[kColumn11Row0Neighbor];
  const FieldElementT& column11_row1 = neighbors[kColumn11Row1Neighbor];
  const FieldElementT& column11_row192 = neighbors[kColumn11Row192Neighbor];
  const FieldElementT& column11_row193 = neighbors[kColumn11Row193Neighbor];
  const FieldElementT& column11_row196 = neighbors[kColumn11Row196Neighbor];
  const FieldElementT& column11_row197 = neighbors[kColumn11Row197Neighbor];
  const FieldElementT& column11_row251 = neighbors[kColumn11Row251Neighbor];
  const FieldElementT& column11_row252 = neighbors[kColumn11Row252Neighbor];
  const FieldElementT& column11_row256 = neighbors[kColumn11Row256Neighbor];
  const FieldElementT& column12_row0 = neighbors[kColumn12Row0Neighbor];
  const FieldElementT& column12_row1 = neighbors[kColumn12Row1Neighbor];
  const FieldElementT& column12_row255 = neighbors[kColumn12Row255Neighbor];
  const FieldElementT& column12_row256 = neighbors[kColumn12Row256Neighbor];
  const FieldElementT& column12_row511 = neighbors[kColumn12Row511Neighbor];
  const FieldElementT& column13_row0 = neighbors[kColumn13Row0Neighbor];
  const FieldElementT& column13_row1 = neighbors[kColumn13Row1Neighbor];
  const FieldElementT& column13_row255 = neighbors[kColumn13Row255Neighbor];
  const FieldElementT& column13_row256 = neighbors[kColumn13Row256Neighbor];
  const FieldElementT& column14_row0 = neighbors[kColumn14Row0Neighbor];
  const FieldElementT& column14_row1 = neighbors[kColumn14Row1Neighbor];
  const FieldElementT& column14_row192 = neighbors[kColumn14Row192Neighbor];
  const FieldElementT& column14_row193 = neighbors[kColumn14Row193Neighbor];
  const FieldElementT& column14_row196 = neighbors[kColumn14Row196Neighbor];
  const FieldElementT& column14_row197 = neighbors[kColumn14Row197Neighbor];
  const FieldElementT& column14_row251 = neighbors[kColumn14Row251Neighbor];
  const FieldElementT& column14_row252 = neighbors[kColumn14Row252Neighbor];
  const FieldElementT& column14_row256 = neighbors[kColumn14Row256Neighbor];
  const FieldElementT& column15_row0 = neighbors[kColumn15Row0Neighbor];
  const FieldElementT& column15_row255 = neighbors[kColumn15Row255Neighbor];
  const FieldElementT& column16_row0 = neighbors[kColumn16Row0Neighbor];
  const FieldElementT& column16_row255 = neighbors[kColumn16Row255Neighbor];
  const FieldElementT& column17_row0 = neighbors[kColumn17Row0Neighbor];
  const FieldElementT& column17_row255 = neighbors[kColumn17Row255Neighbor];
  const FieldElementT& column18_row0 = neighbors[kColumn18Row0Neighbor];
  const FieldElementT& column18_row255 = neighbors[kColumn18Row255Neighbor];
  const FieldElementT& column19_row0 = neighbors[kColumn19Row0Neighbor];
  const FieldElementT& column19_row1 = neighbors[kColumn19Row1Neighbor];
  const FieldElementT& column19_row2 = neighbors[kColumn19Row2Neighbor];
  const FieldElementT& column19_row3 = neighbors[kColumn19Row3Neighbor];
  const FieldElementT& column19_row4 = neighbors[kColumn19Row4Neighbor];
  const FieldElementT& column19_row5 = neighbors[kColumn19Row5Neighbor];
  const FieldElementT& column19_row8 = neighbors[kColumn19Row8Neighbor];
  const FieldElementT& column19_row9 = neighbors[kColumn19Row9Neighbor];
  const FieldElementT& column19_row10 = neighbors[kColumn19Row10Neighbor];
  const FieldElementT& column19_row11 = neighbors[kColumn19Row11Neighbor];
  const FieldElementT& column19_row12 = neighbors[kColumn19Row12Neighbor];
  const FieldElementT& column19_row13 = neighbors[kColumn19Row13Neighbor];
  const FieldElementT& column19_row16 = neighbors[kColumn19Row16Neighbor];
  const FieldElementT& column19_row26 = neighbors[kColumn19Row26Neighbor];
  const FieldElementT& column19_row27 = neighbors[kColumn19Row27Neighbor];
  const FieldElementT& column19_row42 = neighbors[kColumn19Row42Neighbor];
  const FieldElementT& column19_row43 = neighbors[kColumn19Row43Neighbor];
  const FieldElementT& column19_row74 = neighbors[kColumn19Row74Neighbor];
  const FieldElementT& column19_row75 = neighbors[kColumn19Row75Neighbor];
  const FieldElementT& column19_row106 = neighbors[kColumn19Row106Neighbor];
  const FieldElementT& column19_row107 = neighbors[kColumn19Row107Neighbor];
  const FieldElementT& column19_row138 = neighbors[kColumn19Row138Neighbor];
  const FieldElementT& column19_row139 = neighbors[kColumn19Row139Neighbor];
  const FieldElementT& column19_row171 = neighbors[kColumn19Row171Neighbor];
  const FieldElementT& column19_row203 = neighbors[kColumn19Row203Neighbor];
  const FieldElementT& column19_row234 = neighbors[kColumn19Row234Neighbor];
  const FieldElementT& column19_row267 = neighbors[kColumn19Row267Neighbor];
  const FieldElementT& column19_row282 = neighbors[kColumn19Row282Neighbor];
  const FieldElementT& column19_row283 = neighbors[kColumn19Row283Neighbor];
  const FieldElementT& column19_row299 = neighbors[kColumn19Row299Neighbor];
  const FieldElementT& column19_row331 = neighbors[kColumn19Row331Neighbor];
  const FieldElementT& column19_row395 = neighbors[kColumn19Row395Neighbor];
  const FieldElementT& column19_row427 = neighbors[kColumn19Row427Neighbor];
  const FieldElementT& column19_row459 = neighbors[kColumn19Row459Neighbor];
  const FieldElementT& column19_row538 = neighbors[kColumn19Row538Neighbor];
  const FieldElementT& column19_row539 = neighbors[kColumn19Row539Neighbor];
  const FieldElementT& column19_row794 = neighbors[kColumn19Row794Neighbor];
  const FieldElementT& column19_row795 = neighbors[kColumn19Row795Neighbor];
  const FieldElementT& column19_row1050 = neighbors[kColumn19Row1050Neighbor];
  const FieldElementT& column19_row1051 = neighbors[kColumn19Row1051Neighbor];
  const FieldElementT& column19_row1306 = neighbors[kColumn19Row1306Neighbor];
  const FieldElementT& column19_row1307 = neighbors[kColumn19Row1307Neighbor];
  const FieldElementT& column19_row1562 = neighbors[kColumn19Row1562Neighbor];
  const FieldElementT& column19_row2074 = neighbors[kColumn19Row2074Neighbor];
  const FieldElementT& column19_row2075 = neighbors[kColumn19Row2075Neighbor];
  const FieldElementT& column19_row2330 = neighbors[kColumn19Row2330Neighbor];
  const FieldElementT& column19_row2331 = neighbors[kColumn19Row2331Neighbor];
  const FieldElementT& column19_row2587 = neighbors[kColumn19Row2587Neighbor];
  const FieldElementT& column19_row3098 = neighbors[kColumn19Row3098Neighbor];
  const FieldElementT& column19_row3099 = neighbors[kColumn19Row3099Neighbor];
  const FieldElementT& column19_row3354 = neighbors[kColumn19Row3354Neighbor];
  const FieldElementT& column19_row3355 = neighbors[kColumn19Row3355Neighbor];
  const FieldElementT& column19_row3610 = neighbors[kColumn19Row3610Neighbor];
  const FieldElementT& column19_row3611 = neighbors[kColumn19Row3611Neighbor];
  const FieldElementT& column19_row4122 = neighbors[kColumn19Row4122Neighbor];
  const FieldElementT& column19_row4123 = neighbors[kColumn19Row4123Neighbor];
  const FieldElementT& column19_row4634 = neighbors[kColumn19Row4634Neighbor];
  const FieldElementT& column19_row5146 = neighbors[kColumn19Row5146Neighbor];
  const FieldElementT& column19_row8218 = neighbors[kColumn19Row8218Neighbor];
  const FieldElementT& column20_row0 = neighbors[kColumn20Row0Neighbor];
  const FieldElementT& column20_row1 = neighbors[kColumn20Row1Neighbor];
  const FieldElementT& column20_row2 = neighbors[kColumn20Row2Neighbor];
  const FieldElementT& column20_row3 = neighbors[kColumn20Row3Neighbor];
  const FieldElementT& column20_row4 = neighbors[kColumn20Row4Neighbor];
  const FieldElementT& column20_row8 = neighbors[kColumn20Row8Neighbor];
  const FieldElementT& column20_row12 = neighbors[kColumn20Row12Neighbor];
  const FieldElementT& column20_row28 = neighbors[kColumn20Row28Neighbor];
  const FieldElementT& column20_row44 = neighbors[kColumn20Row44Neighbor];
  const FieldElementT& column20_row60 = neighbors[kColumn20Row60Neighbor];
  const FieldElementT& column20_row76 = neighbors[kColumn20Row76Neighbor];
  const FieldElementT& column20_row92 = neighbors[kColumn20Row92Neighbor];
  const FieldElementT& column20_row108 = neighbors[kColumn20Row108Neighbor];
  const FieldElementT& column20_row124 = neighbors[kColumn20Row124Neighbor];
  const FieldElementT& column21_row0 = neighbors[kColumn21Row0Neighbor];
  const FieldElementT& column21_row1 = neighbors[kColumn21Row1Neighbor];
  const FieldElementT& column21_row2 = neighbors[kColumn21Row2Neighbor];
  const FieldElementT& column21_row3 = neighbors[kColumn21Row3Neighbor];
  const FieldElementT& column22_row0 = neighbors[kColumn22Row0Neighbor];
  const FieldElementT& column22_row1 = neighbors[kColumn22Row1Neighbor];
  const FieldElementT& column22_row2 = neighbors[kColumn22Row2Neighbor];
  const FieldElementT& column22_row3 = neighbors[kColumn22Row3Neighbor];
  const FieldElementT& column22_row4 = neighbors[kColumn22Row4Neighbor];
  const FieldElementT& column22_row5 = neighbors[kColumn22Row5Neighbor];
  const FieldElementT& column22_row6 = neighbors[kColumn22Row6Neighbor];
  const FieldElementT& column22_row7 = neighbors[kColumn22Row7Neighbor];
  const FieldElementT& column22_row8 = neighbors[kColumn22Row8Neighbor];
  const FieldElementT& column22_row9 = neighbors[kColumn22Row9Neighbor];
  const FieldElementT& column22_row10 = neighbors[kColumn22Row10Neighbor];
  const FieldElementT& column22_row11 = neighbors[kColumn22Row11Neighbor];
  const FieldElementT& column22_row12 = neighbors[kColumn22Row12Neighbor];
  const FieldElementT& column22_row13 = neighbors[kColumn22Row13Neighbor];
  const FieldElementT& column22_row14 = neighbors[kColumn22Row14Neighbor];
  const FieldElementT& column22_row15 = neighbors[kColumn22Row15Neighbor];
  const FieldElementT& column22_row16 = neighbors[kColumn22Row16Neighbor];
  const FieldElementT& column22_row17 = neighbors[kColumn22Row17Neighbor];
  const FieldElementT& column22_row19 = neighbors[kColumn22Row19Neighbor];
  const FieldElementT& column22_row21 = neighbors[kColumn22Row21Neighbor];
  const FieldElementT& column22_row22 = neighbors[kColumn22Row22Neighbor];
  const FieldElementT& column22_row23 = neighbors[kColumn22Row23Neighbor];
  const FieldElementT& column22_row24 = neighbors[kColumn22Row24Neighbor];
  const FieldElementT& column22_row25 = neighbors[kColumn22Row25Neighbor];
  const FieldElementT& column22_row29 = neighbors[kColumn22Row29Neighbor];
  const FieldElementT& column22_row30 = neighbors[kColumn22Row30Neighbor];
  const FieldElementT& column22_row31 = neighbors[kColumn22Row31Neighbor];
  const FieldElementT& column22_row4081 = neighbors[kColumn22Row4081Neighbor];
  const FieldElementT& column22_row4087 = neighbors[kColumn22Row4087Neighbor];
  const FieldElementT& column22_row4089 = neighbors[kColumn22Row4089Neighbor];
  const FieldElementT& column22_row4095 = neighbors[kColumn22Row4095Neighbor];
  const FieldElementT& column22_row4102 = neighbors[kColumn22Row4102Neighbor];
  const FieldElementT& column22_row4110 = neighbors[kColumn22Row4110Neighbor];
  const FieldElementT& column22_row8177 = neighbors[kColumn22Row8177Neighbor];
  const FieldElementT& column22_row8185 = neighbors[kColumn22Row8185Neighbor];
  const FieldElementT& column23_row0 = neighbors[kColumn23Row0Neighbor];
  const FieldElementT& column23_row1 = neighbors[kColumn23Row1Neighbor];
  const FieldElementT& column23_row2 = neighbors[kColumn23Row2Neighbor];
  const FieldElementT& column23_row4 = neighbors[kColumn23Row4Neighbor];
  const FieldElementT& column23_row6 = neighbors[kColumn23Row6Neighbor];
  const FieldElementT& column23_row8 = neighbors[kColumn23Row8Neighbor];
  const FieldElementT& column23_row10 = neighbors[kColumn23Row10Neighbor];
  const FieldElementT& column23_row12 = neighbors[kColumn23Row12Neighbor];
  const FieldElementT& column23_row14 = neighbors[kColumn23Row14Neighbor];
  const FieldElementT& column23_row16 = neighbors[kColumn23Row16Neighbor];
  const FieldElementT& column23_row17 = neighbors[kColumn23Row17Neighbor];
  const FieldElementT& column23_row22 = neighbors[kColumn23Row22Neighbor];
  const FieldElementT& column23_row30 = neighbors[kColumn23Row30Neighbor];
  const FieldElementT& column23_row38 = neighbors[kColumn23Row38Neighbor];
  const FieldElementT& column23_row46 = neighbors[kColumn23Row46Neighbor];
  const FieldElementT& column23_row54 = neighbors[kColumn23Row54Neighbor];
  const FieldElementT& column23_row81 = neighbors[kColumn23Row81Neighbor];
  const FieldElementT& column23_row145 = neighbors[kColumn23Row145Neighbor];
  const FieldElementT& column23_row209 = neighbors[kColumn23Row209Neighbor];
  const FieldElementT& column23_row3072 = neighbors[kColumn23Row3072Neighbor];
  const FieldElementT& column23_row3088 = neighbors[kColumn23Row3088Neighbor];
  const FieldElementT& column23_row3136 = neighbors[kColumn23Row3136Neighbor];
  const FieldElementT& column23_row3152 = neighbors[kColumn23Row3152Neighbor];
  const FieldElementT& column23_row4016 = neighbors[kColumn23Row4016Neighbor];
  const FieldElementT& column23_row4032 = neighbors[kColumn23Row4032Neighbor];
  const FieldElementT& column23_row4082 = neighbors[kColumn23Row4082Neighbor];
  const FieldElementT& column23_row4084 = neighbors[kColumn23Row4084Neighbor];
  const FieldElementT& column23_row4088 = neighbors[kColumn23Row4088Neighbor];
  const FieldElementT& column23_row4090 = neighbors[kColumn23Row4090Neighbor];
  const FieldElementT& column23_row4092 = neighbors[kColumn23Row4092Neighbor];
  const FieldElementT& column23_row8161 = neighbors[kColumn23Row8161Neighbor];
  const FieldElementT& column23_row8166 = neighbors[kColumn23Row8166Neighbor];
  const FieldElementT& column23_row8178 = neighbors[kColumn23Row8178Neighbor];
  const FieldElementT& column23_row8182 = neighbors[kColumn23Row8182Neighbor];
  const FieldElementT& column23_row8186 = neighbors[kColumn23Row8186Neighbor];
  const FieldElementT& column23_row8190 = neighbors[kColumn23Row8190Neighbor];
  const FieldElementT& column24_inter1_row0 = neighbors[kColumn24Inter1Row0Neighbor];
  const FieldElementT& column24_inter1_row1 = neighbors[kColumn24Inter1Row1Neighbor];
  const FieldElementT& column25_inter1_row0 = neighbors[kColumn25Inter1Row0Neighbor];
  const FieldElementT& column25_inter1_row1 = neighbors[kColumn25Inter1Row1Neighbor];
  const FieldElementT& column26_inter1_row0 = neighbors[kColumn26Inter1Row0Neighbor];
  const FieldElementT& column26_inter1_row1 = neighbors[kColumn26Inter1Row1Neighbor];
  const FieldElementT& column26_inter1_row2 = neighbors[kColumn26Inter1Row2Neighbor];
  const FieldElementT& column26_inter1_row3 = neighbors[kColumn26Inter1Row3Neighbor];

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
      ((column19_row0) + (cpu__decode__opcode_rc__bit_2)) + (FieldElementT::One());
  const FieldElementT cpu__decode__opcode_rc__bit_10 =
      (column0_row10) - ((column0_row11) + (column0_row11));
  const FieldElementT cpu__decode__opcode_rc__bit_11 =
      (column0_row11) - ((column0_row12) + (column0_row12));
  const FieldElementT cpu__decode__opcode_rc__bit_14 =
      (column0_row14) - ((column0_row15) + (column0_row15));
  const FieldElementT memory__address_diff_0 = (column20_row3) - (column20_row1);
  const FieldElementT rc16__diff_0 = (column21_row3) - (column21_row1);
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_0 =
      (column5_row0) - ((column5_row1) + (column5_row1));
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash0__ec_subset_sum__bit_0);
  const FieldElementT pedersen__hash1__ec_subset_sum__bit_0 =
      (column8_row0) - ((column8_row1) + (column8_row1));
  const FieldElementT pedersen__hash1__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash1__ec_subset_sum__bit_0);
  const FieldElementT pedersen__hash2__ec_subset_sum__bit_0 =
      (column11_row0) - ((column11_row1) + (column11_row1));
  const FieldElementT pedersen__hash2__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash2__ec_subset_sum__bit_0);
  const FieldElementT pedersen__hash3__ec_subset_sum__bit_0 =
      (column14_row0) - ((column14_row1) + (column14_row1));
  const FieldElementT pedersen__hash3__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash3__ec_subset_sum__bit_0);
  const FieldElementT rc_builtin__value0_0 = column20_row12;
  const FieldElementT rc_builtin__value1_0 =
      ((rc_builtin__value0_0) * (offset_size_)) + (column20_row28);
  const FieldElementT rc_builtin__value2_0 =
      ((rc_builtin__value1_0) * (offset_size_)) + (column20_row44);
  const FieldElementT rc_builtin__value3_0 =
      ((rc_builtin__value2_0) * (offset_size_)) + (column20_row60);
  const FieldElementT rc_builtin__value4_0 =
      ((rc_builtin__value3_0) * (offset_size_)) + (column20_row76);
  const FieldElementT rc_builtin__value5_0 =
      ((rc_builtin__value4_0) * (offset_size_)) + (column20_row92);
  const FieldElementT rc_builtin__value6_0 =
      ((rc_builtin__value5_0) * (offset_size_)) + (column20_row108);
  const FieldElementT rc_builtin__value7_0 =
      ((rc_builtin__value6_0) * (offset_size_)) + (column20_row124);
  const FieldElementT ecdsa__signature0__doubling_key__x_squared =
      (column22_row6) * (column22_row6);
  const FieldElementT ecdsa__signature0__exponentiate_generator__bit_0 =
      (column23_row14) - ((column23_row46) + (column23_row46));
  const FieldElementT ecdsa__signature0__exponentiate_generator__bit_neg_0 =
      (FieldElementT::One()) - (ecdsa__signature0__exponentiate_generator__bit_0);
  const FieldElementT ecdsa__signature0__exponentiate_key__bit_0 =
      (column22_row5) - ((column22_row21) + (column22_row21));
  const FieldElementT ecdsa__signature0__exponentiate_key__bit_neg_0 =
      (FieldElementT::One()) - (ecdsa__signature0__exponentiate_key__bit_0);
  const FieldElementT bitwise__sum_var_0_0 =
      (((((((column1_row0) + ((column1_row64) * (FieldElementT::ConstexprFromBigInt(0x2_Z)))) +
           ((column1_row128) * (FieldElementT::ConstexprFromBigInt(0x4_Z)))) +
          ((column1_row192) * (FieldElementT::ConstexprFromBigInt(0x8_Z)))) +
         ((column1_row256) * (FieldElementT::ConstexprFromBigInt(0x10000000000000000_Z)))) +
        ((column1_row320) * (FieldElementT::ConstexprFromBigInt(0x20000000000000000_Z)))) +
       ((column1_row384) * (FieldElementT::ConstexprFromBigInt(0x40000000000000000_Z)))) +
      ((column1_row448) * (FieldElementT::ConstexprFromBigInt(0x80000000000000000_Z)));
  const FieldElementT bitwise__sum_var_8_0 =
      ((((((((column1_row512) *
             (FieldElementT::ConstexprFromBigInt(0x100000000000000000000000000000000_Z))) +
            ((column1_row576) *
             (FieldElementT::ConstexprFromBigInt(0x200000000000000000000000000000000_Z)))) +
           ((column1_row640) *
            (FieldElementT::ConstexprFromBigInt(0x400000000000000000000000000000000_Z)))) +
          ((column1_row704) *
           (FieldElementT::ConstexprFromBigInt(0x800000000000000000000000000000000_Z)))) +
         ((column1_row768) * (FieldElementT::ConstexprFromBigInt(
                                 0x1000000000000000000000000000000000000000000000000_Z)))) +
        ((column1_row832) * (FieldElementT::ConstexprFromBigInt(
                                0x2000000000000000000000000000000000000000000000000_Z)))) +
       ((column1_row896) * (FieldElementT::ConstexprFromBigInt(
                               0x4000000000000000000000000000000000000000000000000_Z)))) +
      ((column1_row960) *
       (FieldElementT::ConstexprFromBigInt(0x8000000000000000000000000000000000000000000000000_Z)));
  const FieldElementT ec_op__doubling_q__x_squared_0 = (column22_row13) * (column22_row13);
  const FieldElementT ec_op__ec_subset_sum__bit_0 =
      (column23_row0) - ((column23_row16) + (column23_row16));
  const FieldElementT ec_op__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (ec_op__ec_subset_sum__bit_0);
  FractionFieldElement<FieldElementT> res(FieldElementT::Zero());
  {
    // Compute a sum of constraints with denominator = domain0.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain2.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/decode/opcode_rc/bit:
        const FieldElementT constraint =
            ((cpu__decode__opcode_rc__bit_0) * (cpu__decode__opcode_rc__bit_0)) -
            (cpu__decode__opcode_rc__bit_0);
        inner_sum += random_coefficients[0] * constraint;
      }
      outer_sum += inner_sum * domain2;
    }

    {
      // Compute a sum of constraints with numerator = domain21.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for diluted_check/permutation/step0:
        const FieldElementT constraint =
            (((diluted_check__permutation__interaction_elm_) - (column2_row1)) *
             (column25_inter1_row1)) -
            (((diluted_check__permutation__interaction_elm_) - (column1_row1)) *
             (column25_inter1_row0));
        inner_sum += random_coefficients[48] * constraint;
      }
      {
        // Constraint expression for diluted_check/step:
        const FieldElementT constraint =
            (column24_inter1_row1) -
            (((column24_inter1_row0) *
              ((FieldElementT::One()) +
               ((diluted_check__interaction_z_) * ((column2_row1) - (column2_row0))))) +
             (((diluted_check__interaction_alpha_) * ((column2_row1) - (column2_row0))) *
              ((column2_row1) - (column2_row0))));
        inner_sum += random_coefficients[52] * constraint;
      }
      outer_sum += inner_sum * domain21;
    }

    {
      // Compute a sum of constraints with numerator = domain6.
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
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column4_row0) - (pedersen__points__y))) -
            ((column15_row0) * ((column3_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[63] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column15_row0) * (column15_row0)) -
            ((pedersen__hash0__ec_subset_sum__bit_0) *
             (((column3_row0) + (pedersen__points__x)) + (column3_row1)));
        inner_sum += random_coefficients[64] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column4_row0) + (column4_row1))) -
            ((column15_row0) * ((column3_row0) - (column3_row1)));
        inner_sum += random_coefficients[65] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column3_row1) - (column3_row0));
        inner_sum += random_coefficients[66] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column4_row1) - (column4_row0));
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
            ((pedersen__hash1__ec_subset_sum__bit_0) * ((column7_row0) - (pedersen__points__y))) -
            ((column16_row0) * ((column6_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[81] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column16_row0) * (column16_row0)) -
            ((pedersen__hash1__ec_subset_sum__bit_0) *
             (((column6_row0) + (pedersen__points__x)) + (column6_row1)));
        inner_sum += random_coefficients[82] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash1__ec_subset_sum__bit_0) * ((column7_row0) + (column7_row1))) -
            ((column16_row0) * ((column6_row0) - (column6_row1)));
        inner_sum += random_coefficients[83] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash1__ec_subset_sum__bit_neg_0) * ((column6_row1) - (column6_row0));
        inner_sum += random_coefficients[84] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash1__ec_subset_sum__bit_neg_0) * ((column7_row1) - (column7_row0));
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
            ((pedersen__hash2__ec_subset_sum__bit_0) * ((column10_row0) - (pedersen__points__y))) -
            ((column17_row0) * ((column9_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[99] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column17_row0) * (column17_row0)) -
            ((pedersen__hash2__ec_subset_sum__bit_0) *
             (((column9_row0) + (pedersen__points__x)) + (column9_row1)));
        inner_sum += random_coefficients[100] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash2__ec_subset_sum__bit_0) * ((column10_row0) + (column10_row1))) -
            ((column17_row0) * ((column9_row0) - (column9_row1)));
        inner_sum += random_coefficients[101] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash2__ec_subset_sum__bit_neg_0) * ((column9_row1) - (column9_row0));
        inner_sum += random_coefficients[102] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash2__ec_subset_sum__bit_neg_0) * ((column10_row1) - (column10_row0));
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
            ((pedersen__hash3__ec_subset_sum__bit_0) * ((column13_row0) - (pedersen__points__y))) -
            ((column18_row0) * ((column12_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[117] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column18_row0) * (column18_row0)) -
            ((pedersen__hash3__ec_subset_sum__bit_0) *
             (((column12_row0) + (pedersen__points__x)) + (column12_row1)));
        inner_sum += random_coefficients[118] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash3__ec_subset_sum__bit_0) * ((column13_row0) + (column13_row1))) -
            ((column18_row0) * ((column12_row0) - (column12_row1)));
        inner_sum += random_coefficients[119] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash3__ec_subset_sum__bit_neg_0) * ((column12_row1) - (column12_row0));
        inner_sum += random_coefficients[120] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash3__ec_subset_sum__bit_neg_0) * ((column13_row1) - (column13_row0));
        inner_sum += random_coefficients[121] * constraint;
      }
      outer_sum += inner_sum * domain6;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain0);
  }

  {
    // Compute a sum of constraints with denominator = domain2.
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
    res += FractionFieldElement<FieldElementT>(outer_sum, domain2);
  }

  {
    // Compute a sum of constraints with denominator = domain3.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/decode/opcode_rc_input:
        const FieldElementT constraint =
            (column19_row1) -
            (((((((column0_row0) * (offset_size_)) + (column20_row4)) * (offset_size_)) +
               (column20_row8)) *
              (offset_size_)) +
             (column20_row0));
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
            ((column19_row8) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_0) * (column22_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_0)) * (column22_row0))) +
             (column20_row0));
        inner_sum += random_coefficients[7] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem0_addr:
        const FieldElementT constraint =
            ((column19_row4) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_1) * (column22_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_1)) * (column22_row0))) +
             (column20_row8));
        inner_sum += random_coefficients[8] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem1_addr:
        const FieldElementT constraint = ((column19_row12) + (half_offset_size_)) -
                                         ((((((cpu__decode__opcode_rc__bit_2) * (column19_row0)) +
                                             ((cpu__decode__opcode_rc__bit_4) * (column22_row0))) +
                                            ((cpu__decode__opcode_rc__bit_3) * (column22_row8))) +
                                           ((cpu__decode__flag_op1_base_op0_0) * (column19_row5))) +
                                          (column20_row4));
        inner_sum += random_coefficients[9] * constraint;
      }
      {
        // Constraint expression for cpu/operands/ops_mul:
        const FieldElementT constraint = (column22_row4) - ((column19_row5) * (column19_row13));
        inner_sum += random_coefficients[10] * constraint;
      }
      {
        // Constraint expression for cpu/operands/res:
        const FieldElementT constraint =
            (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column22_row12)) -
            ((((cpu__decode__opcode_rc__bit_5) * ((column19_row5) + (column19_row13))) +
              ((cpu__decode__opcode_rc__bit_6) * (column22_row4))) +
             ((cpu__decode__flag_res_op1_0) * (column19_row13)));
        inner_sum += random_coefficients[11] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_fp:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column19_row9) - (column22_row8));
        inner_sum += random_coefficients[18] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_pc:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) *
            ((column19_row5) -
             (((column19_row0) + (cpu__decode__opcode_rc__bit_2)) + (FieldElementT::One())));
        inner_sum += random_coefficients[19] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off0:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column20_row0) - (half_offset_size_));
        inner_sum += random_coefficients[20] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off1:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) *
            ((column20_row8) - ((half_offset_size_) + (FieldElementT::One())));
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
            (((column20_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z))) - (half_offset_size_));
        inner_sum += random_coefficients[23] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/off2:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_13) *
            (((column20_row4) + (FieldElementT::One())) - (half_offset_size_));
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
            (cpu__decode__opcode_rc__bit_14) * ((column19_row9) - (column22_row12));
        inner_sum += random_coefficients[26] * constraint;
      }
      {
        // Constraint expression for public_memory_addr_zero:
        const FieldElementT constraint = column19_row2;
        inner_sum += random_coefficients[39] * constraint;
      }
      {
        // Constraint expression for public_memory_value_zero:
        const FieldElementT constraint = column19_row3;
        inner_sum += random_coefficients[40] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain22.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp0:
        const FieldElementT constraint =
            (column22_row2) - ((cpu__decode__opcode_rc__bit_9) * (column19_row9));
        inner_sum += random_coefficients[12] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp1:
        const FieldElementT constraint = (column22_row10) - ((column22_row2) * (column22_row12));
        inner_sum += random_coefficients[13] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_negative:
        const FieldElementT constraint =
            ((((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column19_row16)) +
             ((column22_row2) * ((column19_row16) - ((column19_row0) + (column19_row13))))) -
            ((((cpu__decode__flag_pc_update_regular_0) * (npc_reg_0)) +
              ((cpu__decode__opcode_rc__bit_7) * (column22_row12))) +
             ((cpu__decode__opcode_rc__bit_8) * ((column19_row0) + (column22_row12))));
        inner_sum += random_coefficients[14] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_positive:
        const FieldElementT constraint =
            ((column22_row10) - (cpu__decode__opcode_rc__bit_9)) * ((column19_row16) - (npc_reg_0));
        inner_sum += random_coefficients[15] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_ap/ap_update:
        const FieldElementT constraint =
            (column22_row16) -
            ((((column22_row0) + ((cpu__decode__opcode_rc__bit_10) * (column22_row12))) +
              (cpu__decode__opcode_rc__bit_11)) +
             ((cpu__decode__opcode_rc__bit_12) * (FieldElementT::ConstexprFromBigInt(0x2_Z))));
        inner_sum += random_coefficients[16] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_fp/fp_update:
        const FieldElementT constraint =
            (column22_row24) - ((((cpu__decode__fp_update_regular_0) * (column22_row8)) +
                                 ((cpu__decode__opcode_rc__bit_13) * (column19_row9))) +
                                ((cpu__decode__opcode_rc__bit_12) *
                                 ((column22_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z)))));
        inner_sum += random_coefficients[17] * constraint;
      }
      outer_sum += inner_sum * domain22;
    }

    {
      // Compute a sum of constraints with numerator = domain12.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/doubling_key/slope:
        const FieldElementT constraint = ((((ecdsa__signature0__doubling_key__x_squared) +
                                            (ecdsa__signature0__doubling_key__x_squared)) +
                                           (ecdsa__signature0__doubling_key__x_squared)) +
                                          ((ecdsa__sig_config_).alpha)) -
                                         (((column22_row14) + (column22_row14)) * (column23_row8));
        inner_sum += random_coefficients[145] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/doubling_key/x:
        const FieldElementT constraint = ((column23_row8) * (column23_row8)) -
                                         (((column22_row6) + (column22_row6)) + (column22_row22));
        inner_sum += random_coefficients[146] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/doubling_key/y:
        const FieldElementT constraint = ((column22_row14) + (column22_row30)) -
                                         ((column23_row8) * ((column22_row6) - (column22_row22)));
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
            ((ecdsa__signature0__exponentiate_key__bit_0) * ((column22_row9) - (column22_row14))) -
            ((column23_row4) * ((column22_row1) - (column22_row6)));
        inner_sum += random_coefficients[160] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/x:
        const FieldElementT constraint = ((column23_row4) * (column23_row4)) -
                                         ((ecdsa__signature0__exponentiate_key__bit_0) *
                                          (((column22_row1) + (column22_row6)) + (column22_row17)));
        inner_sum += random_coefficients[161] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/y:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_key__bit_0) * ((column22_row9) + (column22_row25))) -
            ((column23_row4) * ((column22_row1) - (column22_row17)));
        inner_sum += random_coefficients[162] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column23_row12) * ((column22_row1) - (column22_row6))) - (FieldElementT::One());
        inner_sum += random_coefficients[163] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/copy_point/x:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_neg_0) * ((column22_row17) - (column22_row1));
        inner_sum += random_coefficients[164] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/copy_point/y:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_neg_0) * ((column22_row25) - (column22_row9));
        inner_sum += random_coefficients[165] * constraint;
      }
      {
        // Constraint expression for ec_op/doubling_q/slope:
        const FieldElementT constraint =
            ((((ec_op__doubling_q__x_squared_0) + (ec_op__doubling_q__x_squared_0)) +
              (ec_op__doubling_q__x_squared_0)) +
             ((ec_op__curve_config_).alpha)) -
            (((column22_row3) + (column22_row3)) * (column22_row11));
        inner_sum += random_coefficients[205] * constraint;
      }
      {
        // Constraint expression for ec_op/doubling_q/x:
        const FieldElementT constraint = ((column22_row11) * (column22_row11)) -
                                         (((column22_row13) + (column22_row13)) + (column22_row29));
        inner_sum += random_coefficients[206] * constraint;
      }
      {
        // Constraint expression for ec_op/doubling_q/y:
        const FieldElementT constraint = ((column22_row3) + (column22_row19)) -
                                         ((column22_row11) * ((column22_row13) - (column22_row29)));
        inner_sum += random_coefficients[207] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/booleanity_test:
        const FieldElementT constraint = (ec_op__ec_subset_sum__bit_0) *
                                         ((ec_op__ec_subset_sum__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[216] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/add_points/slope:
        const FieldElementT constraint =
            ((ec_op__ec_subset_sum__bit_0) * ((column22_row15) - (column22_row3))) -
            ((column23_row2) * ((column22_row7) - (column22_row13)));
        inner_sum += random_coefficients[219] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column23_row2) * (column23_row2)) -
            ((ec_op__ec_subset_sum__bit_0) *
             (((column22_row7) + (column22_row13)) + (column22_row23)));
        inner_sum += random_coefficients[220] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((ec_op__ec_subset_sum__bit_0) * ((column22_row15) + (column22_row31))) -
            ((column23_row2) * ((column22_row7) - (column22_row23)));
        inner_sum += random_coefficients[221] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column23_row10) * ((column22_row7) - (column22_row13))) - (FieldElementT::One());
        inner_sum += random_coefficients[222] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (ec_op__ec_subset_sum__bit_neg_0) * ((column22_row23) - (column22_row7));
        inner_sum += random_coefficients[223] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (ec_op__ec_subset_sum__bit_neg_0) * ((column22_row31) - (column22_row15));
        inner_sum += random_coefficients[224] * constraint;
      }
      outer_sum += inner_sum * domain12;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain3);
  }

  {
    // Compute a sum of constraints with denominator = domain23.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for initial_ap:
        const FieldElementT constraint = (column22_row0) - (initial_ap_);
        inner_sum += random_coefficients[27] * constraint;
      }
      {
        // Constraint expression for initial_fp:
        const FieldElementT constraint = (column22_row8) - (initial_ap_);
        inner_sum += random_coefficients[28] * constraint;
      }
      {
        // Constraint expression for initial_pc:
        const FieldElementT constraint = (column19_row0) - (initial_pc_);
        inner_sum += random_coefficients[29] * constraint;
      }
      {
        // Constraint expression for memory/multi_column_perm/perm/init0:
        const FieldElementT constraint =
            (((((memory__multi_column_perm__perm__interaction_elm_) -
                ((column20_row1) +
                 ((memory__multi_column_perm__hash_interaction_elm0_) * (column21_row0)))) *
               (column26_inter1_row0)) +
              (column19_row0)) +
             ((memory__multi_column_perm__hash_interaction_elm0_) * (column19_row1))) -
            (memory__multi_column_perm__perm__interaction_elm_);
        inner_sum += random_coefficients[33] * constraint;
      }
      {
        // Constraint expression for memory/initial_addr:
        const FieldElementT constraint = (column20_row1) - (FieldElementT::One());
        inner_sum += random_coefficients[38] * constraint;
      }
      {
        // Constraint expression for rc16/perm/init0:
        const FieldElementT constraint =
            ((((rc16__perm__interaction_elm_) - (column21_row1)) * (column26_inter1_row1)) +
             (column20_row0)) -
            (rc16__perm__interaction_elm_);
        inner_sum += random_coefficients[41] * constraint;
      }
      {
        // Constraint expression for rc16/minimum:
        const FieldElementT constraint = (column21_row1) - (rc_min_);
        inner_sum += random_coefficients[45] * constraint;
      }
      {
        // Constraint expression for diluted_check/permutation/init0:
        const FieldElementT constraint =
            ((((diluted_check__permutation__interaction_elm_) - (column2_row0)) *
              (column25_inter1_row0)) +
             (column1_row0)) -
            (diluted_check__permutation__interaction_elm_);
        inner_sum += random_coefficients[47] * constraint;
      }
      {
        // Constraint expression for diluted_check/init:
        const FieldElementT constraint = (column24_inter1_row0) - (FieldElementT::One());
        inner_sum += random_coefficients[50] * constraint;
      }
      {
        // Constraint expression for diluted_check/first_element:
        const FieldElementT constraint = (column2_row0) - (diluted_check__first_elm_);
        inner_sum += random_coefficients[51] * constraint;
      }
      {
        // Constraint expression for pedersen/init_addr:
        const FieldElementT constraint = (column19_row10) - (initial_pedersen_addr_);
        inner_sum += random_coefficients[131] * constraint;
      }
      {
        // Constraint expression for rc_builtin/init_addr:
        const FieldElementT constraint = (column19_row106) - (initial_rc_addr_);
        inner_sum += random_coefficients[144] * constraint;
      }
      {
        // Constraint expression for ecdsa/init_addr:
        const FieldElementT constraint = (column19_row26) - (initial_ecdsa_addr_);
        inner_sum += random_coefficients[181] * constraint;
      }
      {
        // Constraint expression for bitwise/init_var_pool_addr:
        const FieldElementT constraint = (column19_row538) - (initial_bitwise_addr_);
        inner_sum += random_coefficients[186] * constraint;
      }
      {
        // Constraint expression for ec_op/init_addr:
        const FieldElementT constraint = (column19_row1050) - (initial_ec_op_addr_);
        inner_sum += random_coefficients[197] * constraint;
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
        const FieldElementT constraint = (column22_row0) - (final_ap_);
        inner_sum += random_coefficients[30] * constraint;
      }
      {
        // Constraint expression for final_fp:
        const FieldElementT constraint = (column22_row8) - (initial_ap_);
        inner_sum += random_coefficients[31] * constraint;
      }
      {
        // Constraint expression for final_pc:
        const FieldElementT constraint = (column19_row0) - (final_pc_);
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
              ((column20_row3) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column21_row2)))) *
             (column26_inter1_row2)) -
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column19_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column19_row3)))) *
             (column26_inter1_row0));
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
                                         ((column21_row0) - (column21_row2));
        inner_sum += random_coefficients[37] * constraint;
      }
      {
        // Constraint expression for rc16/perm/step0:
        const FieldElementT constraint =
            (((rc16__perm__interaction_elm_) - (column21_row3)) * (column26_inter1_row3)) -
            (((rc16__perm__interaction_elm_) - (column20_row2)) * (column26_inter1_row1));
        inner_sum += random_coefficients[42] * constraint;
      }
      {
        // Constraint expression for rc16/diff_is_bit:
        const FieldElementT constraint = ((rc16__diff_0) * (rc16__diff_0)) - (rc16__diff_0);
        inner_sum += random_coefficients[44] * constraint;
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
            (column26_inter1_row0) - (memory__multi_column_perm__perm__public_memory_prod_);
        inner_sum += random_coefficients[35] * constraint;
      }
      {
        // Constraint expression for rc16/perm/last:
        const FieldElementT constraint = (column26_inter1_row1) - (rc16__perm__public_memory_prod_);
        inner_sum += random_coefficients[43] * constraint;
      }
      {
        // Constraint expression for rc16/maximum:
        const FieldElementT constraint = (column21_row1) - (rc_max_);
        inner_sum += random_coefficients[46] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain24);
  }

  {
    // Compute a sum of constraints with denominator = domain21.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for diluted_check/permutation/last:
        const FieldElementT constraint =
            (column25_inter1_row0) - (diluted_check__permutation__public_memory_prod_);
        inner_sum += random_coefficients[49] * constraint;
      }
      {
        // Constraint expression for diluted_check/last:
        const FieldElementT constraint = (column24_inter1_row0) - (diluted_check__final_cum_val_);
        inner_sum += random_coefficients[53] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain21);
  }

  {
    // Compute a sum of constraints with denominator = domain7.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column16_row255) * ((column5_row0) - ((column5_row1) + (column5_row1)));
        inner_sum += random_coefficients[54] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column16_row255) *
            ((column5_row1) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column5_row192)));
        inner_sum += random_coefficients[55] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column16_row255) -
            ((column15_row255) * ((column5_row192) - ((column5_row193) + (column5_row193))));
        inner_sum += random_coefficients[56] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column15_row255) *
            ((column5_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column5_row196)));
        inner_sum += random_coefficients[57] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column15_row255) - (((column5_row251) - ((column5_row252) + (column5_row252))) *
                                 ((column5_row196) - ((column5_row197) + (column5_row197))));
        inner_sum += random_coefficients[58] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column5_row251) - ((column5_row252) + (column5_row252))) *
            ((column5_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column5_row251)));
        inner_sum += random_coefficients[59] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column18_row255) * ((column8_row0) - ((column8_row1) + (column8_row1)));
        inner_sum += random_coefficients[72] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash1/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column18_row255) *
            ((column8_row1) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column8_row192)));
        inner_sum += random_coefficients[73] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column18_row255) -
            ((column17_row255) * ((column8_row192) - ((column8_row193) + (column8_row193))));
        inner_sum += random_coefficients[74] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash1/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column17_row255) *
            ((column8_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column8_row196)));
        inner_sum += random_coefficients[75] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column17_row255) - (((column8_row251) - ((column8_row252) + (column8_row252))) *
                                 ((column8_row196) - ((column8_row197) + (column8_row197))));
        inner_sum += random_coefficients[76] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash1/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column8_row251) - ((column8_row252) + (column8_row252))) *
            ((column8_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column8_row251)));
        inner_sum += random_coefficients[77] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column23_row145) * ((column11_row0) - ((column11_row1) + (column11_row1)));
        inner_sum += random_coefficients[90] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash2/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column23_row145) *
            ((column11_row1) - ((FieldElementT::ConstexprFromBigInt(
                                    0x800000000000000000000000000000000000000000000000_Z)) *
                                (column11_row192)));
        inner_sum += random_coefficients[91] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column23_row145) -
            ((column23_row17) * ((column11_row192) - ((column11_row193) + (column11_row193))));
        inner_sum += random_coefficients[92] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash2/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column23_row17) *
            ((column11_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column11_row196)));
        inner_sum += random_coefficients[93] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column23_row17) - (((column11_row251) - ((column11_row252) + (column11_row252))) *
                                ((column11_row196) - ((column11_row197) + (column11_row197))));
        inner_sum += random_coefficients[94] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash2/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column11_row251) - ((column11_row252) + (column11_row252))) *
            ((column11_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column11_row251)));
        inner_sum += random_coefficients[95] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column23_row209) * ((column14_row0) - ((column14_row1) + (column14_row1)));
        inner_sum += random_coefficients[108] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash3/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column23_row209) *
            ((column14_row1) - ((FieldElementT::ConstexprFromBigInt(
                                    0x800000000000000000000000000000000000000000000000_Z)) *
                                (column14_row192)));
        inner_sum += random_coefficients[109] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column23_row209) -
            ((column23_row81) * ((column14_row192) - ((column14_row193) + (column14_row193))));
        inner_sum += random_coefficients[110] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash3/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column23_row81) *
            ((column14_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column14_row196)));
        inner_sum += random_coefficients[111] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column23_row81) - (((column14_row251) - ((column14_row252) + (column14_row252))) *
                                ((column14_row196) - ((column14_row197) + (column14_row197))));
        inner_sum += random_coefficients[112] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash3/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column14_row251) - ((column14_row252) + (column14_row252))) *
            ((column14_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column14_row251)));
        inner_sum += random_coefficients[113] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain9.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/copy_point/x:
        const FieldElementT constraint = (column3_row256) - (column3_row255);
        inner_sum += random_coefficients[68] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/copy_point/y:
        const FieldElementT constraint = (column4_row256) - (column4_row255);
        inner_sum += random_coefficients[69] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/copy_point/x:
        const FieldElementT constraint = (column6_row256) - (column6_row255);
        inner_sum += random_coefficients[86] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/copy_point/y:
        const FieldElementT constraint = (column7_row256) - (column7_row255);
        inner_sum += random_coefficients[87] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/copy_point/x:
        const FieldElementT constraint = (column9_row256) - (column9_row255);
        inner_sum += random_coefficients[104] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/copy_point/y:
        const FieldElementT constraint = (column10_row256) - (column10_row255);
        inner_sum += random_coefficients[105] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/copy_point/x:
        const FieldElementT constraint = (column12_row256) - (column12_row255);
        inner_sum += random_coefficients[122] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/copy_point/y:
        const FieldElementT constraint = (column13_row256) - (column13_row255);
        inner_sum += random_coefficients[123] * constraint;
      }
      outer_sum += inner_sum * domain9;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain7);
  }

  {
    // Compute a sum of constraints with denominator = domain8.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column5_row0;
        inner_sum += random_coefficients[61] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column8_row0;
        inner_sum += random_coefficients[79] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column11_row0;
        inner_sum += random_coefficients[97] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column14_row0;
        inner_sum += random_coefficients[115] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain8);
  }

  {
    // Compute a sum of constraints with denominator = domain6.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column5_row0;
        inner_sum += random_coefficients[62] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column8_row0;
        inner_sum += random_coefficients[80] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column11_row0;
        inner_sum += random_coefficients[98] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column14_row0;
        inner_sum += random_coefficients[116] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain6);
  }

  {
    // Compute a sum of constraints with denominator = domain10.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/init/x:
        const FieldElementT constraint = (column3_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[70] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/init/y:
        const FieldElementT constraint = (column4_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[71] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/init/x:
        const FieldElementT constraint = (column6_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[88] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/init/y:
        const FieldElementT constraint = (column7_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[89] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/init/x:
        const FieldElementT constraint = (column9_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[106] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/init/y:
        const FieldElementT constraint = (column10_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[107] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/init/x:
        const FieldElementT constraint = (column12_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[124] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/init/y:
        const FieldElementT constraint = (column13_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[125] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value0:
        const FieldElementT constraint = (column19_row11) - (column5_row0);
        inner_sum += random_coefficients[126] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value1:
        const FieldElementT constraint = (column19_row139) - (column8_row0);
        inner_sum += random_coefficients[127] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value2:
        const FieldElementT constraint = (column19_row267) - (column11_row0);
        inner_sum += random_coefficients[128] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value3:
        const FieldElementT constraint = (column19_row395) - (column14_row0);
        inner_sum += random_coefficients[129] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value0:
        const FieldElementT constraint = (column19_row75) - (column5_row256);
        inner_sum += random_coefficients[132] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value1:
        const FieldElementT constraint = (column19_row203) - (column8_row256);
        inner_sum += random_coefficients[133] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value2:
        const FieldElementT constraint = (column19_row331) - (column11_row256);
        inner_sum += random_coefficients[134] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value3:
        const FieldElementT constraint = (column19_row459) - (column14_row256);
        inner_sum += random_coefficients[135] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value0:
        const FieldElementT constraint = (column19_row43) - (column3_row511);
        inner_sum += random_coefficients[137] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value1:
        const FieldElementT constraint = (column19_row171) - (column6_row511);
        inner_sum += random_coefficients[138] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value2:
        const FieldElementT constraint = (column19_row299) - (column9_row511);
        inner_sum += random_coefficients[139] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value3:
        const FieldElementT constraint = (column19_row427) - (column12_row511);
        inner_sum += random_coefficients[140] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain10);
  }

  {
    // Compute a sum of constraints with denominator = domain5.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain25.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input0_addr:
        const FieldElementT constraint =
            (column19_row138) - ((column19_row42) + (FieldElementT::One()));
        inner_sum += random_coefficients[130] * constraint;
      }
      {
        // Constraint expression for rc_builtin/addr_step:
        const FieldElementT constraint =
            (column19_row234) - ((column19_row106) + (FieldElementT::One()));
        inner_sum += random_coefficients[143] * constraint;
      }
      outer_sum += inner_sum * domain25;
    }

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input1_addr:
        const FieldElementT constraint =
            (column19_row74) - ((column19_row10) + (FieldElementT::One()));
        inner_sum += random_coefficients[136] * constraint;
      }
      {
        // Constraint expression for pedersen/output_addr:
        const FieldElementT constraint =
            (column19_row42) - ((column19_row74) + (FieldElementT::One()));
        inner_sum += random_coefficients[141] * constraint;
      }
      {
        // Constraint expression for rc_builtin/value:
        const FieldElementT constraint = (rc_builtin__value7_0) - (column19_row107);
        inner_sum += random_coefficients[142] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain5);
  }

  {
    // Compute a sum of constraints with denominator = domain4.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain18.
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
             ((column23_row22) - (ecdsa__generator_points__y))) -
            ((column23_row30) * ((column23_row6) - (ecdsa__generator_points__x)));
        inner_sum += random_coefficients[151] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/x:
        const FieldElementT constraint =
            ((column23_row30) * (column23_row30)) -
            ((ecdsa__signature0__exponentiate_generator__bit_0) *
             (((column23_row6) + (ecdsa__generator_points__x)) + (column23_row38)));
        inner_sum += random_coefficients[152] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/y:
        const FieldElementT constraint = ((ecdsa__signature0__exponentiate_generator__bit_0) *
                                          ((column23_row22) + (column23_row54))) -
                                         ((column23_row30) * ((column23_row6) - (column23_row38)));
        inner_sum += random_coefficients[153] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column23_row1) * ((column23_row6) - (ecdsa__generator_points__x))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[154] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/copy_point/x:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_generator__bit_neg_0) *
                                         ((column23_row38) - (column23_row6));
        inner_sum += random_coefficients[155] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/copy_point/y:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_generator__bit_neg_0) *
                                         ((column23_row54) - (column23_row22));
        inner_sum += random_coefficients[156] * constraint;
      }
      outer_sum += inner_sum * domain18;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain4);
  }

  {
    // Compute a sum of constraints with denominator = domain19.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/bit_extraction_end:
        const FieldElementT constraint = column23_row14;
        inner_sum += random_coefficients[149] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain19);
  }

  {
    // Compute a sum of constraints with denominator = domain18.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/zeros_tail:
        const FieldElementT constraint = column23_row14;
        inner_sum += random_coefficients[150] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain18);
  }

  {
    // Compute a sum of constraints with denominator = domain13.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/bit_extraction_end:
        const FieldElementT constraint = column22_row5;
        inner_sum += random_coefficients[158] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain13);
  }

  {
    // Compute a sum of constraints with denominator = domain12.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/zeros_tail:
        const FieldElementT constraint = column22_row5;
        inner_sum += random_coefficients[159] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column23_row0;
        inner_sum += random_coefficients[218] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain12);
  }

  {
    // Compute a sum of constraints with denominator = domain20.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/init_gen/x:
        const FieldElementT constraint = (column23_row6) - (((ecdsa__sig_config_).shift_point).x);
        inner_sum += random_coefficients[166] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/init_gen/y:
        const FieldElementT constraint = (column23_row22) + (((ecdsa__sig_config_).shift_point).y);
        inner_sum += random_coefficients[167] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/slope:
        const FieldElementT constraint =
            (column23_row8182) -
            ((column22_row4089) + ((column23_row8190) * ((column23_row8166) - (column22_row4081))));
        inner_sum += random_coefficients[170] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/x:
        const FieldElementT constraint =
            ((column23_row8190) * (column23_row8190)) -
            (((column23_row8166) + (column22_row4081)) + (column22_row4102));
        inner_sum += random_coefficients[171] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/y:
        const FieldElementT constraint =
            ((column23_row8182) + (column22_row4110)) -
            ((column23_row8190) * ((column23_row8166) - (column22_row4102)));
        inner_sum += random_coefficients[172] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/x_diff_inv:
        const FieldElementT constraint =
            ((column23_row8161) * ((column23_row8166) - (column22_row4081))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[173] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/slope:
        const FieldElementT constraint =
            ((column22_row8185) + (((ecdsa__sig_config_).shift_point).y)) -
            ((column23_row4082) * ((column22_row8177) - (((ecdsa__sig_config_).shift_point).x)));
        inner_sum += random_coefficients[174] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/x:
        const FieldElementT constraint =
            ((column23_row4082) * (column23_row4082)) -
            (((column22_row8177) + (((ecdsa__sig_config_).shift_point).x)) + (column22_row5));
        inner_sum += random_coefficients[175] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/x_diff_inv:
        const FieldElementT constraint =
            ((column23_row8178) * ((column22_row8177) - (((ecdsa__sig_config_).shift_point).x))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[176] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/z_nonzero:
        const FieldElementT constraint =
            ((column23_row14) * (column23_row4090)) - (FieldElementT::One());
        inner_sum += random_coefficients[177] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/q_on_curve/x_squared:
        const FieldElementT constraint = (column23_row8186) - ((column22_row6) * (column22_row6));
        inner_sum += random_coefficients[179] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/q_on_curve/on_curve:
        const FieldElementT constraint = ((column22_row14) * (column22_row14)) -
                                         ((((column22_row6) * (column23_row8186)) +
                                           (((ecdsa__sig_config_).alpha) * (column22_row6))) +
                                          ((ecdsa__sig_config_).beta));
        inner_sum += random_coefficients[180] * constraint;
      }
      {
        // Constraint expression for ecdsa/message_addr:
        const FieldElementT constraint =
            (column19_row4122) - ((column19_row26) + (FieldElementT::One()));
        inner_sum += random_coefficients[182] * constraint;
      }
      {
        // Constraint expression for ecdsa/message_value0:
        const FieldElementT constraint = (column19_row4123) - (column23_row14);
        inner_sum += random_coefficients[184] * constraint;
      }
      {
        // Constraint expression for ecdsa/pubkey_value0:
        const FieldElementT constraint = (column19_row27) - (column22_row6);
        inner_sum += random_coefficients[185] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain26.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/pubkey_addr:
        const FieldElementT constraint =
            (column19_row8218) - ((column19_row4122) + (FieldElementT::One()));
        inner_sum += random_coefficients[183] * constraint;
      }
      outer_sum += inner_sum * domain26;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain20);
  }

  {
    // Compute a sum of constraints with denominator = domain14.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/init_key/x:
        const FieldElementT constraint = (column22_row1) - (((ecdsa__sig_config_).shift_point).x);
        inner_sum += random_coefficients[168] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/init_key/y:
        const FieldElementT constraint = (column22_row9) - (((ecdsa__sig_config_).shift_point).y);
        inner_sum += random_coefficients[169] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/r_and_w_nonzero:
        const FieldElementT constraint =
            ((column22_row5) * (column23_row4088)) - (FieldElementT::One());
        inner_sum += random_coefficients[178] * constraint;
      }
      {
        // Constraint expression for bitwise/x_or_y_addr:
        const FieldElementT constraint =
            (column19_row2074) - ((column19_row3610) + (FieldElementT::One()));
        inner_sum += random_coefficients[188] * constraint;
      }
      {
        // Constraint expression for bitwise/or_is_and_plus_xor:
        const FieldElementT constraint =
            (column19_row2075) - ((column19_row2587) + (column19_row3611));
        inner_sum += random_coefficients[191] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking192:
        const FieldElementT constraint = (((column1_row2816) + (column1_row3840)) *
                                          (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
                                         (column1_row32);
        inner_sum += random_coefficients[193] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking193:
        const FieldElementT constraint = (((column1_row2880) + (column1_row3904)) *
                                          (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
                                         (column1_row2080);
        inner_sum += random_coefficients[194] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking194:
        const FieldElementT constraint = (((column1_row2944) + (column1_row3968)) *
                                          (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
                                         (column1_row1056);
        inner_sum += random_coefficients[195] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking195:
        const FieldElementT constraint = (((column1_row3008) + (column1_row4032)) *
                                          (FieldElementT::ConstexprFromBigInt(0x100_Z))) -
                                         (column1_row3104);
        inner_sum += random_coefficients[196] * constraint;
      }
      {
        // Constraint expression for ec_op/p_y_addr:
        const FieldElementT constraint =
            (column19_row3098) - ((column19_row1050) + (FieldElementT::One()));
        inner_sum += random_coefficients[199] * constraint;
      }
      {
        // Constraint expression for ec_op/q_x_addr:
        const FieldElementT constraint =
            (column19_row282) - ((column19_row3098) + (FieldElementT::One()));
        inner_sum += random_coefficients[200] * constraint;
      }
      {
        // Constraint expression for ec_op/q_y_addr:
        const FieldElementT constraint =
            (column19_row2330) - ((column19_row282) + (FieldElementT::One()));
        inner_sum += random_coefficients[201] * constraint;
      }
      {
        // Constraint expression for ec_op/m_addr:
        const FieldElementT constraint =
            (column19_row1306) - ((column19_row2330) + (FieldElementT::One()));
        inner_sum += random_coefficients[202] * constraint;
      }
      {
        // Constraint expression for ec_op/r_x_addr:
        const FieldElementT constraint =
            (column19_row3354) - ((column19_row1306) + (FieldElementT::One()));
        inner_sum += random_coefficients[203] * constraint;
      }
      {
        // Constraint expression for ec_op/r_y_addr:
        const FieldElementT constraint =
            (column19_row794) - ((column19_row3354) + (FieldElementT::One()));
        inner_sum += random_coefficients[204] * constraint;
      }
      {
        // Constraint expression for ec_op/get_q_x:
        const FieldElementT constraint = (column19_row283) - (column22_row13);
        inner_sum += random_coefficients[208] * constraint;
      }
      {
        // Constraint expression for ec_op/get_q_y:
        const FieldElementT constraint = (column19_row2331) - (column22_row3);
        inner_sum += random_coefficients[209] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column23_row4092) * ((column23_row0) - ((column23_row16) + (column23_row16)));
        inner_sum += random_coefficients[210] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column23_row4092) *
            ((column23_row16) - ((FieldElementT::ConstexprFromBigInt(
                                     0x800000000000000000000000000000000000000000000000_Z)) *
                                 (column23_row3072)));
        inner_sum += random_coefficients[211] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column23_row4092) -
            ((column23_row4084) * ((column23_row3072) - ((column23_row3088) + (column23_row3088))));
        inner_sum += random_coefficients[212] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column23_row4084) *
            ((column23_row3088) -
             ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column23_row3136)));
        inner_sum += random_coefficients[213] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column23_row4084) - (((column23_row4016) - ((column23_row4032) + (column23_row4032))) *
                                  ((column23_row3136) - ((column23_row3152) + (column23_row3152))));
        inner_sum += random_coefficients[214] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column23_row4016) - ((column23_row4032) + (column23_row4032))) *
            ((column23_row3152) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column23_row4016)));
        inner_sum += random_coefficients[215] * constraint;
      }
      {
        // Constraint expression for ec_op/get_m:
        const FieldElementT constraint = (column23_row0) - (column19_row1307);
        inner_sum += random_coefficients[225] * constraint;
      }
      {
        // Constraint expression for ec_op/get_p_x:
        const FieldElementT constraint = (column19_row1051) - (column22_row7);
        inner_sum += random_coefficients[226] * constraint;
      }
      {
        // Constraint expression for ec_op/get_p_y:
        const FieldElementT constraint = (column19_row3099) - (column22_row15);
        inner_sum += random_coefficients[227] * constraint;
      }
      {
        // Constraint expression for ec_op/set_r_x:
        const FieldElementT constraint = (column19_row3355) - (column22_row4087);
        inner_sum += random_coefficients[228] * constraint;
      }
      {
        // Constraint expression for ec_op/set_r_y:
        const FieldElementT constraint = (column19_row795) - (column22_row4095);
        inner_sum += random_coefficients[229] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain27.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/next_var_pool_addr:
        const FieldElementT constraint =
            (column19_row4634) - ((column19_row2074) + (FieldElementT::One()));
        inner_sum += random_coefficients[189] * constraint;
      }
      {
        // Constraint expression for ec_op/p_x_addr:
        const FieldElementT constraint =
            (column19_row5146) - ((column19_row1050) + (FieldElementT::ConstexprFromBigInt(0x7_Z)));
        inner_sum += random_coefficients[198] * constraint;
      }
      outer_sum += inner_sum * domain27;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain14);
  }

  {
    // Compute a sum of constraints with denominator = domain11.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain15.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/step_var_pool_addr:
        const FieldElementT constraint =
            (column19_row1562) - ((column19_row538) + (FieldElementT::One()));
        inner_sum += random_coefficients[187] * constraint;
      }
      outer_sum += inner_sum * domain15;
    }

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/partition:
        const FieldElementT constraint =
            ((bitwise__sum_var_0_0) + (bitwise__sum_var_8_0)) - (column19_row539);
        inner_sum += random_coefficients[190] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain11);
  }

  {
    // Compute a sum of constraints with denominator = domain16.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/addition_is_xor_with_and:
        const FieldElementT constraint =
            ((column1_row0) + (column1_row1024)) -
            (((column1_row3072) + (column1_row2048)) + (column1_row2048));
        inner_sum += random_coefficients[192] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain16);
  }

  {
    // Compute a sum of constraints with denominator = domain17.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column23_row0;
        inner_sum += random_coefficients[217] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain17);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> CpuAirDefinition<FieldElementT, 3>::DomainEvalsAtPoint(
    gsl::span<const FieldElementT> point_powers,
    gsl::span<const FieldElementT> shifts) const {
  const FieldElementT& domain0 = (point_powers[1]) - (FieldElementT::One());
  const FieldElementT& domain1 = (point_powers[2]) - (FieldElementT::One());
  const FieldElementT& domain2 = (point_powers[3]) - (shifts[0]);
  const FieldElementT& domain3 = (point_powers[3]) - (FieldElementT::One());
  const FieldElementT& domain4 = (point_powers[4]) - (FieldElementT::One());
  const FieldElementT& domain5 = (point_powers[5]) - (FieldElementT::One());
  const FieldElementT& domain6 = (point_powers[6]) - (shifts[1]);
  const FieldElementT& domain7 = (point_powers[6]) - (FieldElementT::One());
  const FieldElementT& domain8 = (point_powers[6]) - (shifts[2]);
  const FieldElementT& domain9 = (point_powers[7]) - (shifts[3]);
  const FieldElementT& domain10 = (point_powers[7]) - (FieldElementT::One());
  const FieldElementT& domain11 = (point_powers[8]) - (FieldElementT::One());
  const FieldElementT& domain12 = (point_powers[9]) - (shifts[1]);
  const FieldElementT& domain13 = (point_powers[9]) - (shifts[4]);
  const FieldElementT& domain14 = (point_powers[9]) - (FieldElementT::One());
  const FieldElementT& domain15 = (point_powers[9]) - (shifts[5]);
  const FieldElementT& domain16 =
      ((point_powers[9]) - (shifts[6])) * ((point_powers[9]) - (shifts[7])) *
      ((point_powers[9]) - (shifts[8])) * ((point_powers[9]) - (shifts[9])) *
      ((point_powers[9]) - (shifts[10])) * ((point_powers[9]) - (shifts[11])) *
      ((point_powers[9]) - (shifts[12])) * ((point_powers[9]) - (shifts[13])) *
      ((point_powers[9]) - (shifts[14])) * ((point_powers[9]) - (shifts[15])) *
      ((point_powers[9]) - (shifts[16])) * ((point_powers[9]) - (shifts[17])) *
      ((point_powers[9]) - (shifts[18])) * ((point_powers[9]) - (shifts[19])) *
      ((point_powers[9]) - (shifts[20])) * (domain14);
  const FieldElementT& domain17 = (point_powers[9]) - (shifts[2]);
  const FieldElementT& domain18 = (point_powers[10]) - (shifts[1]);
  const FieldElementT& domain19 = (point_powers[10]) - (shifts[4]);
  const FieldElementT& domain20 = (point_powers[10]) - (FieldElementT::One());
  return {
      domain0,  domain1,  domain2,  domain3,  domain4,  domain5,  domain6,
      domain7,  domain8,  domain9,  domain10, domain11, domain12, domain13,
      domain14, domain15, domain16, domain17, domain18, domain19, domain20,
  };
}

template <typename FieldElementT>
TraceGenerationContext CpuAirDefinition<FieldElementT, 3>::GetTraceGenerationContext() const {
  TraceGenerationContext ctx;

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 4096)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 4096)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 4096)) - (1)) <= (SafeDiv(trace_length_, 4096)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 4096)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 4096)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 4096)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 4096)) <= (SafeDiv(trace_length_, 4096)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 4096)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 4096)), "Index out of range.");

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

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 2)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 2)) + (-1)) < (SafeDiv(trace_length_, 2)), "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 2)) + (-1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 2)), "Index out of range.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 2)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 2)) - (1)) <= (SafeDiv(trace_length_, 2)), "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 2)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 2)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 2)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 2)) <= (SafeDiv(trace_length_, 2)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 2)) >= (0), "Index should be non negative.");

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
      "diluted_pool", VirtualColumn(/*column=*/kColumn1Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "diluted_check/permuted_values",
      VirtualColumn(/*column=*/kColumn2Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn9Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn10Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn11Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn12Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn13Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn14Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn15Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn16Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn18Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "mem_pool/addr", VirtualColumn(/*column=*/kColumn19Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "mem_pool/value", VirtualColumn(/*column=*/kColumn19Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "rc16_pool", VirtualColumn(/*column=*/kColumn20Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/sorted/addr",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "memory/sorted/value",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "rc16/sorted", VirtualColumn(/*column=*/kColumn21Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/registers/ap", VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/registers/fp", VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/operands/ops_mul",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/res",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp0",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp1",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/10));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/key_points/x",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/key_points/y",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/14));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/partial_sum/x",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/partial_sum/y",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/selector",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "ec_op/doubled_points/x",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "ec_op/doubled_points/y",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "ec_op/doubling_slope",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/11));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/16, /*row_offset=*/15));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/doubling_slope",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/slope",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/x_diff_inv",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/16, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/x_diff_inv",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/16, /*row_offset=*/10));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/partial_sum/x",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/32, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/partial_sum/y",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/32, /*row_offset=*/22));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/selector",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/32, /*row_offset=*/14));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/slope",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/32, /*row_offset=*/30));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/x_diff_inv",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/32, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn15Column, /*step=*/256, /*row_offset=*/255));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn16Column, /*step=*/256, /*row_offset=*/255));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn17Column, /*step=*/256, /*row_offset=*/255));
  ctx.AddVirtualColumn(
      "pedersen/hash1/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn18Column, /*step=*/256, /*row_offset=*/255));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/256, /*row_offset=*/17));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/256, /*row_offset=*/145));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/256, /*row_offset=*/81));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/256, /*row_offset=*/209));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/r_w_inv",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/4096, /*row_offset=*/4088));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/4096, /*row_offset=*/4084));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/4096, /*row_offset=*/4092));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/add_results_slope",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/8192, /*row_offset=*/8190));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/add_results_inv",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/8192, /*row_offset=*/8161));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/extract_r_slope",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/8192, /*row_offset=*/4082));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/extract_r_inv",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/8192, /*row_offset=*/8178));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/z_inv",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/8192, /*row_offset=*/4090));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/q_x_squared",
      VirtualColumn(/*column=*/kColumn23Column, /*step=*/8192, /*row_offset=*/8186));
  ctx.AddVirtualColumn(
      "diluted_check/cumulative_value",
      VirtualColumn(
          /*column=*/kColumn24Inter1Column - kNumColumnsFirst, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "diluted_check/permutation/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn25Inter1Column - kNumColumnsFirst, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/multi_column_perm/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn26Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "rc16/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn26Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/mem_inst/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/mem_inst/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/pc", VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/instruction",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/off0", VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/off1", VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/decode/off2", VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_dst/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_dst/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op0/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op0/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op1/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op1/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "orig/public_memory/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "orig/public_memory/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/16, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "pedersen/input0/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/10));
  ctx.AddVirtualColumn(
      "pedersen/input0/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/11));
  ctx.AddVirtualColumn(
      "pedersen/input1/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/74));
  ctx.AddVirtualColumn(
      "pedersen/input1/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/75));
  ctx.AddVirtualColumn(
      "pedersen/output/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/42));
  ctx.AddVirtualColumn(
      "pedersen/output/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/43));
  ctx.AddVirtualColumn(
      "rc_builtin/mem/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/106));
  ctx.AddVirtualColumn(
      "rc_builtin/mem/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/107));
  ctx.AddVirtualColumn(
      "rc_builtin/inner_rc",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "ecdsa/pubkey/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8192, /*row_offset=*/26));
  ctx.AddVirtualColumn(
      "ecdsa/pubkey/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8192, /*row_offset=*/27));
  ctx.AddVirtualColumn(
      "ecdsa/message/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8192, /*row_offset=*/4122));
  ctx.AddVirtualColumn(
      "ecdsa/message/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8192, /*row_offset=*/4123));
  ctx.AddVirtualColumn(
      "bitwise/x/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/538));
  ctx.AddVirtualColumn(
      "bitwise/x/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/539));
  ctx.AddVirtualColumn(
      "bitwise/y/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/1562));
  ctx.AddVirtualColumn(
      "bitwise/y/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/1563));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/2586));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/2587));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/3610));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/3611));
  ctx.AddVirtualColumn(
      "bitwise/x_or_y/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/2074));
  ctx.AddVirtualColumn(
      "bitwise/x_or_y/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/2075));
  ctx.AddVirtualColumn(
      "bitwise/diluted_var_pool",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/64, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "bitwise/x", VirtualColumn(/*column=*/kColumn1Column, /*step=*/64, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "bitwise/y", VirtualColumn(/*column=*/kColumn1Column, /*step=*/64, /*row_offset=*/1024));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/64, /*row_offset=*/2048));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/64, /*row_offset=*/3072));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking192",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/4096, /*row_offset=*/32));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking193",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/4096, /*row_offset=*/2080));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking194",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/4096, /*row_offset=*/1056));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking195",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/4096, /*row_offset=*/3104));
  ctx.AddVirtualColumn(
      "ec_op/p_x/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/1050));
  ctx.AddVirtualColumn(
      "ec_op/p_x/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/1051));
  ctx.AddVirtualColumn(
      "ec_op/p_y/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/3098));
  ctx.AddVirtualColumn(
      "ec_op/p_y/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/3099));
  ctx.AddVirtualColumn(
      "ec_op/q_x/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/282));
  ctx.AddVirtualColumn(
      "ec_op/q_x/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/283));
  ctx.AddVirtualColumn(
      "ec_op/q_y/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/2330));
  ctx.AddVirtualColumn(
      "ec_op/q_y/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/2331));
  ctx.AddVirtualColumn(
      "ec_op/m/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/1306));
  ctx.AddVirtualColumn(
      "ec_op/m/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/1307));
  ctx.AddVirtualColumn(
      "ec_op/r_x/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/3354));
  ctx.AddVirtualColumn(
      "ec_op/r_x/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/3355));
  ctx.AddVirtualColumn(
      "ec_op/r_y/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/794));
  ctx.AddVirtualColumn(
      "ec_op/r_y/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/4096, /*row_offset=*/795));

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
  ctx.AddObject<std::vector<size_t>>(
      "ec_op/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "ec_op/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);

  return ctx;
}

template <typename FieldElementT>
std::vector<std::pair<int64_t, uint64_t>> CpuAirDefinition<FieldElementT, 3>::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;

  mask.reserve(286);
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
  mask.emplace_back(32, kColumn1Column);
  mask.emplace_back(64, kColumn1Column);
  mask.emplace_back(128, kColumn1Column);
  mask.emplace_back(192, kColumn1Column);
  mask.emplace_back(256, kColumn1Column);
  mask.emplace_back(320, kColumn1Column);
  mask.emplace_back(384, kColumn1Column);
  mask.emplace_back(448, kColumn1Column);
  mask.emplace_back(512, kColumn1Column);
  mask.emplace_back(576, kColumn1Column);
  mask.emplace_back(640, kColumn1Column);
  mask.emplace_back(704, kColumn1Column);
  mask.emplace_back(768, kColumn1Column);
  mask.emplace_back(832, kColumn1Column);
  mask.emplace_back(896, kColumn1Column);
  mask.emplace_back(960, kColumn1Column);
  mask.emplace_back(1024, kColumn1Column);
  mask.emplace_back(1056, kColumn1Column);
  mask.emplace_back(2048, kColumn1Column);
  mask.emplace_back(2080, kColumn1Column);
  mask.emplace_back(2816, kColumn1Column);
  mask.emplace_back(2880, kColumn1Column);
  mask.emplace_back(2944, kColumn1Column);
  mask.emplace_back(3008, kColumn1Column);
  mask.emplace_back(3072, kColumn1Column);
  mask.emplace_back(3104, kColumn1Column);
  mask.emplace_back(3840, kColumn1Column);
  mask.emplace_back(3904, kColumn1Column);
  mask.emplace_back(3968, kColumn1Column);
  mask.emplace_back(4032, kColumn1Column);
  mask.emplace_back(0, kColumn2Column);
  mask.emplace_back(1, kColumn2Column);
  mask.emplace_back(0, kColumn3Column);
  mask.emplace_back(1, kColumn3Column);
  mask.emplace_back(255, kColumn3Column);
  mask.emplace_back(256, kColumn3Column);
  mask.emplace_back(511, kColumn3Column);
  mask.emplace_back(0, kColumn4Column);
  mask.emplace_back(1, kColumn4Column);
  mask.emplace_back(255, kColumn4Column);
  mask.emplace_back(256, kColumn4Column);
  mask.emplace_back(0, kColumn5Column);
  mask.emplace_back(1, kColumn5Column);
  mask.emplace_back(192, kColumn5Column);
  mask.emplace_back(193, kColumn5Column);
  mask.emplace_back(196, kColumn5Column);
  mask.emplace_back(197, kColumn5Column);
  mask.emplace_back(251, kColumn5Column);
  mask.emplace_back(252, kColumn5Column);
  mask.emplace_back(256, kColumn5Column);
  mask.emplace_back(0, kColumn6Column);
  mask.emplace_back(1, kColumn6Column);
  mask.emplace_back(255, kColumn6Column);
  mask.emplace_back(256, kColumn6Column);
  mask.emplace_back(511, kColumn6Column);
  mask.emplace_back(0, kColumn7Column);
  mask.emplace_back(1, kColumn7Column);
  mask.emplace_back(255, kColumn7Column);
  mask.emplace_back(256, kColumn7Column);
  mask.emplace_back(0, kColumn8Column);
  mask.emplace_back(1, kColumn8Column);
  mask.emplace_back(192, kColumn8Column);
  mask.emplace_back(193, kColumn8Column);
  mask.emplace_back(196, kColumn8Column);
  mask.emplace_back(197, kColumn8Column);
  mask.emplace_back(251, kColumn8Column);
  mask.emplace_back(252, kColumn8Column);
  mask.emplace_back(256, kColumn8Column);
  mask.emplace_back(0, kColumn9Column);
  mask.emplace_back(1, kColumn9Column);
  mask.emplace_back(255, kColumn9Column);
  mask.emplace_back(256, kColumn9Column);
  mask.emplace_back(511, kColumn9Column);
  mask.emplace_back(0, kColumn10Column);
  mask.emplace_back(1, kColumn10Column);
  mask.emplace_back(255, kColumn10Column);
  mask.emplace_back(256, kColumn10Column);
  mask.emplace_back(0, kColumn11Column);
  mask.emplace_back(1, kColumn11Column);
  mask.emplace_back(192, kColumn11Column);
  mask.emplace_back(193, kColumn11Column);
  mask.emplace_back(196, kColumn11Column);
  mask.emplace_back(197, kColumn11Column);
  mask.emplace_back(251, kColumn11Column);
  mask.emplace_back(252, kColumn11Column);
  mask.emplace_back(256, kColumn11Column);
  mask.emplace_back(0, kColumn12Column);
  mask.emplace_back(1, kColumn12Column);
  mask.emplace_back(255, kColumn12Column);
  mask.emplace_back(256, kColumn12Column);
  mask.emplace_back(511, kColumn12Column);
  mask.emplace_back(0, kColumn13Column);
  mask.emplace_back(1, kColumn13Column);
  mask.emplace_back(255, kColumn13Column);
  mask.emplace_back(256, kColumn13Column);
  mask.emplace_back(0, kColumn14Column);
  mask.emplace_back(1, kColumn14Column);
  mask.emplace_back(192, kColumn14Column);
  mask.emplace_back(193, kColumn14Column);
  mask.emplace_back(196, kColumn14Column);
  mask.emplace_back(197, kColumn14Column);
  mask.emplace_back(251, kColumn14Column);
  mask.emplace_back(252, kColumn14Column);
  mask.emplace_back(256, kColumn14Column);
  mask.emplace_back(0, kColumn15Column);
  mask.emplace_back(255, kColumn15Column);
  mask.emplace_back(0, kColumn16Column);
  mask.emplace_back(255, kColumn16Column);
  mask.emplace_back(0, kColumn17Column);
  mask.emplace_back(255, kColumn17Column);
  mask.emplace_back(0, kColumn18Column);
  mask.emplace_back(255, kColumn18Column);
  mask.emplace_back(0, kColumn19Column);
  mask.emplace_back(1, kColumn19Column);
  mask.emplace_back(2, kColumn19Column);
  mask.emplace_back(3, kColumn19Column);
  mask.emplace_back(4, kColumn19Column);
  mask.emplace_back(5, kColumn19Column);
  mask.emplace_back(8, kColumn19Column);
  mask.emplace_back(9, kColumn19Column);
  mask.emplace_back(10, kColumn19Column);
  mask.emplace_back(11, kColumn19Column);
  mask.emplace_back(12, kColumn19Column);
  mask.emplace_back(13, kColumn19Column);
  mask.emplace_back(16, kColumn19Column);
  mask.emplace_back(26, kColumn19Column);
  mask.emplace_back(27, kColumn19Column);
  mask.emplace_back(42, kColumn19Column);
  mask.emplace_back(43, kColumn19Column);
  mask.emplace_back(74, kColumn19Column);
  mask.emplace_back(75, kColumn19Column);
  mask.emplace_back(106, kColumn19Column);
  mask.emplace_back(107, kColumn19Column);
  mask.emplace_back(138, kColumn19Column);
  mask.emplace_back(139, kColumn19Column);
  mask.emplace_back(171, kColumn19Column);
  mask.emplace_back(203, kColumn19Column);
  mask.emplace_back(234, kColumn19Column);
  mask.emplace_back(267, kColumn19Column);
  mask.emplace_back(282, kColumn19Column);
  mask.emplace_back(283, kColumn19Column);
  mask.emplace_back(299, kColumn19Column);
  mask.emplace_back(331, kColumn19Column);
  mask.emplace_back(395, kColumn19Column);
  mask.emplace_back(427, kColumn19Column);
  mask.emplace_back(459, kColumn19Column);
  mask.emplace_back(538, kColumn19Column);
  mask.emplace_back(539, kColumn19Column);
  mask.emplace_back(794, kColumn19Column);
  mask.emplace_back(795, kColumn19Column);
  mask.emplace_back(1050, kColumn19Column);
  mask.emplace_back(1051, kColumn19Column);
  mask.emplace_back(1306, kColumn19Column);
  mask.emplace_back(1307, kColumn19Column);
  mask.emplace_back(1562, kColumn19Column);
  mask.emplace_back(2074, kColumn19Column);
  mask.emplace_back(2075, kColumn19Column);
  mask.emplace_back(2330, kColumn19Column);
  mask.emplace_back(2331, kColumn19Column);
  mask.emplace_back(2587, kColumn19Column);
  mask.emplace_back(3098, kColumn19Column);
  mask.emplace_back(3099, kColumn19Column);
  mask.emplace_back(3354, kColumn19Column);
  mask.emplace_back(3355, kColumn19Column);
  mask.emplace_back(3610, kColumn19Column);
  mask.emplace_back(3611, kColumn19Column);
  mask.emplace_back(4122, kColumn19Column);
  mask.emplace_back(4123, kColumn19Column);
  mask.emplace_back(4634, kColumn19Column);
  mask.emplace_back(5146, kColumn19Column);
  mask.emplace_back(8218, kColumn19Column);
  mask.emplace_back(0, kColumn20Column);
  mask.emplace_back(1, kColumn20Column);
  mask.emplace_back(2, kColumn20Column);
  mask.emplace_back(3, kColumn20Column);
  mask.emplace_back(4, kColumn20Column);
  mask.emplace_back(8, kColumn20Column);
  mask.emplace_back(12, kColumn20Column);
  mask.emplace_back(28, kColumn20Column);
  mask.emplace_back(44, kColumn20Column);
  mask.emplace_back(60, kColumn20Column);
  mask.emplace_back(76, kColumn20Column);
  mask.emplace_back(92, kColumn20Column);
  mask.emplace_back(108, kColumn20Column);
  mask.emplace_back(124, kColumn20Column);
  mask.emplace_back(0, kColumn21Column);
  mask.emplace_back(1, kColumn21Column);
  mask.emplace_back(2, kColumn21Column);
  mask.emplace_back(3, kColumn21Column);
  mask.emplace_back(0, kColumn22Column);
  mask.emplace_back(1, kColumn22Column);
  mask.emplace_back(2, kColumn22Column);
  mask.emplace_back(3, kColumn22Column);
  mask.emplace_back(4, kColumn22Column);
  mask.emplace_back(5, kColumn22Column);
  mask.emplace_back(6, kColumn22Column);
  mask.emplace_back(7, kColumn22Column);
  mask.emplace_back(8, kColumn22Column);
  mask.emplace_back(9, kColumn22Column);
  mask.emplace_back(10, kColumn22Column);
  mask.emplace_back(11, kColumn22Column);
  mask.emplace_back(12, kColumn22Column);
  mask.emplace_back(13, kColumn22Column);
  mask.emplace_back(14, kColumn22Column);
  mask.emplace_back(15, kColumn22Column);
  mask.emplace_back(16, kColumn22Column);
  mask.emplace_back(17, kColumn22Column);
  mask.emplace_back(19, kColumn22Column);
  mask.emplace_back(21, kColumn22Column);
  mask.emplace_back(22, kColumn22Column);
  mask.emplace_back(23, kColumn22Column);
  mask.emplace_back(24, kColumn22Column);
  mask.emplace_back(25, kColumn22Column);
  mask.emplace_back(29, kColumn22Column);
  mask.emplace_back(30, kColumn22Column);
  mask.emplace_back(31, kColumn22Column);
  mask.emplace_back(4081, kColumn22Column);
  mask.emplace_back(4087, kColumn22Column);
  mask.emplace_back(4089, kColumn22Column);
  mask.emplace_back(4095, kColumn22Column);
  mask.emplace_back(4102, kColumn22Column);
  mask.emplace_back(4110, kColumn22Column);
  mask.emplace_back(8177, kColumn22Column);
  mask.emplace_back(8185, kColumn22Column);
  mask.emplace_back(0, kColumn23Column);
  mask.emplace_back(1, kColumn23Column);
  mask.emplace_back(2, kColumn23Column);
  mask.emplace_back(4, kColumn23Column);
  mask.emplace_back(6, kColumn23Column);
  mask.emplace_back(8, kColumn23Column);
  mask.emplace_back(10, kColumn23Column);
  mask.emplace_back(12, kColumn23Column);
  mask.emplace_back(14, kColumn23Column);
  mask.emplace_back(16, kColumn23Column);
  mask.emplace_back(17, kColumn23Column);
  mask.emplace_back(22, kColumn23Column);
  mask.emplace_back(30, kColumn23Column);
  mask.emplace_back(38, kColumn23Column);
  mask.emplace_back(46, kColumn23Column);
  mask.emplace_back(54, kColumn23Column);
  mask.emplace_back(81, kColumn23Column);
  mask.emplace_back(145, kColumn23Column);
  mask.emplace_back(209, kColumn23Column);
  mask.emplace_back(3072, kColumn23Column);
  mask.emplace_back(3088, kColumn23Column);
  mask.emplace_back(3136, kColumn23Column);
  mask.emplace_back(3152, kColumn23Column);
  mask.emplace_back(4016, kColumn23Column);
  mask.emplace_back(4032, kColumn23Column);
  mask.emplace_back(4082, kColumn23Column);
  mask.emplace_back(4084, kColumn23Column);
  mask.emplace_back(4088, kColumn23Column);
  mask.emplace_back(4090, kColumn23Column);
  mask.emplace_back(4092, kColumn23Column);
  mask.emplace_back(8161, kColumn23Column);
  mask.emplace_back(8166, kColumn23Column);
  mask.emplace_back(8178, kColumn23Column);
  mask.emplace_back(8182, kColumn23Column);
  mask.emplace_back(8186, kColumn23Column);
  mask.emplace_back(8190, kColumn23Column);
  mask.emplace_back(0, kColumn24Inter1Column);
  mask.emplace_back(1, kColumn24Inter1Column);
  mask.emplace_back(0, kColumn25Inter1Column);
  mask.emplace_back(1, kColumn25Inter1Column);
  mask.emplace_back(0, kColumn26Inter1Column);
  mask.emplace_back(1, kColumn26Inter1Column);
  mask.emplace_back(2, kColumn26Inter1Column);
  mask.emplace_back(3, kColumn26Inter1Column);

  return mask;
}

}  // namespace cpu

}  // namespace starkware
