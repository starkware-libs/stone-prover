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
CpuAirDefinition<FieldElementT, 2>::CreateCompositionPolynomial(
    const FieldElement& trace_generator, const ConstFieldElementSpan& random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const FieldElementT& gen = trace_generator.As<FieldElementT>();

  const std::vector<uint64_t> point_exponents = {
      trace_length_,
      SafeDiv(trace_length_, 2),
      SafeDiv(trace_length_, 4),
      SafeDiv(trace_length_, 8),
      SafeDiv(trace_length_, 16),
      SafeDiv(trace_length_, 64),
      SafeDiv(trace_length_, 128),
      SafeDiv(trace_length_, 256),
      SafeDiv(trace_length_, 512),
      SafeDiv(trace_length_, 16384),
      SafeDiv(trace_length_, 32768)};
  const std::vector<uint64_t> gen_exponents = {
      SafeDiv((15) * (trace_length_), 16),
      SafeDiv((255) * (trace_length_), 256),
      SafeDiv((63) * (trace_length_), 64),
      SafeDiv(trace_length_, 2),
      SafeDiv((251) * (trace_length_), 256),
      (16) * ((SafeDiv(trace_length_, 16)) - (1)),
      (2) * ((SafeDiv(trace_length_, 2)) - (1)),
      (4) * ((SafeDiv(trace_length_, 4)) - (1)),
      (256) * ((SafeDiv(trace_length_, 256)) - (1)),
      (512) * ((SafeDiv(trace_length_, 512)) - (1)),
      (32768) * ((SafeDiv(trace_length_, 32768)) - (1))};

  BuildPeriodicColumns(gen, &builder);

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
CpuAirDefinition<FieldElementT, 2>::PrecomputeDomainEvalsOnCoset(
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
      FieldElementT::UninitializedVector(1),     FieldElementT::UninitializedVector(2),
      FieldElementT::UninitializedVector(4),     FieldElementT::UninitializedVector(8),
      FieldElementT::UninitializedVector(16),    FieldElementT::UninitializedVector(16),
      FieldElementT::UninitializedVector(64),    FieldElementT::UninitializedVector(128),
      FieldElementT::UninitializedVector(256),   FieldElementT::UninitializedVector(256),
      FieldElementT::UninitializedVector(256),   FieldElementT::UninitializedVector(512),
      FieldElementT::UninitializedVector(512),   FieldElementT::UninitializedVector(16384),
      FieldElementT::UninitializedVector(16384), FieldElementT::UninitializedVector(16384),
      FieldElementT::UninitializedVector(32768), FieldElementT::UninitializedVector(32768),
      FieldElementT::UninitializedVector(32768),
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

  period = 64;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[6][i] = (point_powers[5][i & (63)]) - (FieldElementT::One());
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

  period = 16384;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[13][i] = (point_powers[9][i & (16383)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 16384;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[14][i] = (point_powers[9][i & (16383)]) - (shifts[4]);
        }
      },
      period, kTaskSize);

  period = 16384;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[15][i] = (point_powers[9][i & (16383)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 32768;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[16][i] = (point_powers[10][i & (32767)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 32768;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[17][i] = (point_powers[10][i & (32767)]) - (shifts[4]);
        }
      },
      period, kTaskSize);

  period = 32768;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[18][i] = (point_powers[10][i & (32767)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);
  return precomp_domains;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> CpuAirDefinition<FieldElementT, 2>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 11, "shifts should contain 11 elements.");

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
  // domain6 = point^(trace_length / 64) - 1.
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
  // domain13 = point^(trace_length / 16384) - gen^(255 * trace_length / 256).
  const FieldElementT& domain13 = precomp_domains[13];
  // domain14 = point^(trace_length / 16384) - gen^(251 * trace_length / 256).
  const FieldElementT& domain14 = precomp_domains[14];
  // domain15 = point^(trace_length / 16384) - 1.
  const FieldElementT& domain15 = precomp_domains[15];
  // domain16 = point^(trace_length / 32768) - gen^(255 * trace_length / 256).
  const FieldElementT& domain16 = precomp_domains[16];
  // domain17 = point^(trace_length / 32768) - gen^(251 * trace_length / 256).
  const FieldElementT& domain17 = precomp_domains[17];
  // domain18 = point^(trace_length / 32768) - 1.
  const FieldElementT& domain18 = precomp_domains[18];
  // domain19 = point - gen^(16 * (trace_length / 16 - 1)).
  const FieldElementT& domain19 = (point) - (shifts[5]);
  // domain20 = point - 1.
  const FieldElementT& domain20 = (point) - (FieldElementT::One());
  // domain21 = point - gen^(2 * (trace_length / 2 - 1)).
  const FieldElementT& domain21 = (point) - (shifts[6]);
  // domain22 = point - gen^(4 * (trace_length / 4 - 1)).
  const FieldElementT& domain22 = (point) - (shifts[7]);
  // domain23 = point - gen^(256 * (trace_length / 256 - 1)).
  const FieldElementT& domain23 = (point) - (shifts[8]);
  // domain24 = point - gen^(512 * (trace_length / 512 - 1)).
  const FieldElementT& domain24 = (point) - (shifts[9]);
  // domain25 = point - gen^(32768 * (trace_length / 32768 - 1)).
  const FieldElementT& domain25 = (point) - (shifts[10]);

  ASSERT_VERIFIER(neighbors.size() == 128, "Neighbors must contain 128 elements.");
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
  const FieldElementT& column4_row255 = neighbors[kColumn4Row255Neighbor];
  const FieldElementT& column5_row0 = neighbors[kColumn5Row0Neighbor];
  const FieldElementT& column5_row1 = neighbors[kColumn5Row1Neighbor];
  const FieldElementT& column5_row2 = neighbors[kColumn5Row2Neighbor];
  const FieldElementT& column5_row3 = neighbors[kColumn5Row3Neighbor];
  const FieldElementT& column5_row4 = neighbors[kColumn5Row4Neighbor];
  const FieldElementT& column5_row5 = neighbors[kColumn5Row5Neighbor];
  const FieldElementT& column5_row6 = neighbors[kColumn5Row6Neighbor];
  const FieldElementT& column5_row7 = neighbors[kColumn5Row7Neighbor];
  const FieldElementT& column5_row8 = neighbors[kColumn5Row8Neighbor];
  const FieldElementT& column5_row9 = neighbors[kColumn5Row9Neighbor];
  const FieldElementT& column5_row12 = neighbors[kColumn5Row12Neighbor];
  const FieldElementT& column5_row13 = neighbors[kColumn5Row13Neighbor];
  const FieldElementT& column5_row16 = neighbors[kColumn5Row16Neighbor];
  const FieldElementT& column5_row70 = neighbors[kColumn5Row70Neighbor];
  const FieldElementT& column5_row71 = neighbors[kColumn5Row71Neighbor];
  const FieldElementT& column5_row134 = neighbors[kColumn5Row134Neighbor];
  const FieldElementT& column5_row135 = neighbors[kColumn5Row135Neighbor];
  const FieldElementT& column5_row262 = neighbors[kColumn5Row262Neighbor];
  const FieldElementT& column5_row263 = neighbors[kColumn5Row263Neighbor];
  const FieldElementT& column5_row326 = neighbors[kColumn5Row326Neighbor];
  const FieldElementT& column5_row390 = neighbors[kColumn5Row390Neighbor];
  const FieldElementT& column5_row391 = neighbors[kColumn5Row391Neighbor];
  const FieldElementT& column5_row518 = neighbors[kColumn5Row518Neighbor];
  const FieldElementT& column5_row16774 = neighbors[kColumn5Row16774Neighbor];
  const FieldElementT& column5_row16775 = neighbors[kColumn5Row16775Neighbor];
  const FieldElementT& column5_row33158 = neighbors[kColumn5Row33158Neighbor];
  const FieldElementT& column6_row0 = neighbors[kColumn6Row0Neighbor];
  const FieldElementT& column6_row1 = neighbors[kColumn6Row1Neighbor];
  const FieldElementT& column6_row2 = neighbors[kColumn6Row2Neighbor];
  const FieldElementT& column6_row3 = neighbors[kColumn6Row3Neighbor];
  const FieldElementT& column7_row0 = neighbors[kColumn7Row0Neighbor];
  const FieldElementT& column7_row1 = neighbors[kColumn7Row1Neighbor];
  const FieldElementT& column7_row2 = neighbors[kColumn7Row2Neighbor];
  const FieldElementT& column7_row3 = neighbors[kColumn7Row3Neighbor];
  const FieldElementT& column7_row4 = neighbors[kColumn7Row4Neighbor];
  const FieldElementT& column7_row5 = neighbors[kColumn7Row5Neighbor];
  const FieldElementT& column7_row6 = neighbors[kColumn7Row6Neighbor];
  const FieldElementT& column7_row7 = neighbors[kColumn7Row7Neighbor];
  const FieldElementT& column7_row8 = neighbors[kColumn7Row8Neighbor];
  const FieldElementT& column7_row9 = neighbors[kColumn7Row9Neighbor];
  const FieldElementT& column7_row11 = neighbors[kColumn7Row11Neighbor];
  const FieldElementT& column7_row12 = neighbors[kColumn7Row12Neighbor];
  const FieldElementT& column7_row13 = neighbors[kColumn7Row13Neighbor];
  const FieldElementT& column7_row15 = neighbors[kColumn7Row15Neighbor];
  const FieldElementT& column7_row17 = neighbors[kColumn7Row17Neighbor];
  const FieldElementT& column7_row23 = neighbors[kColumn7Row23Neighbor];
  const FieldElementT& column7_row25 = neighbors[kColumn7Row25Neighbor];
  const FieldElementT& column7_row31 = neighbors[kColumn7Row31Neighbor];
  const FieldElementT& column7_row39 = neighbors[kColumn7Row39Neighbor];
  const FieldElementT& column7_row44 = neighbors[kColumn7Row44Neighbor];
  const FieldElementT& column7_row47 = neighbors[kColumn7Row47Neighbor];
  const FieldElementT& column7_row55 = neighbors[kColumn7Row55Neighbor];
  const FieldElementT& column7_row63 = neighbors[kColumn7Row63Neighbor];
  const FieldElementT& column7_row71 = neighbors[kColumn7Row71Neighbor];
  const FieldElementT& column7_row76 = neighbors[kColumn7Row76Neighbor];
  const FieldElementT& column7_row79 = neighbors[kColumn7Row79Neighbor];
  const FieldElementT& column7_row87 = neighbors[kColumn7Row87Neighbor];
  const FieldElementT& column7_row103 = neighbors[kColumn7Row103Neighbor];
  const FieldElementT& column7_row108 = neighbors[kColumn7Row108Neighbor];
  const FieldElementT& column7_row119 = neighbors[kColumn7Row119Neighbor];
  const FieldElementT& column7_row140 = neighbors[kColumn7Row140Neighbor];
  const FieldElementT& column7_row172 = neighbors[kColumn7Row172Neighbor];
  const FieldElementT& column7_row204 = neighbors[kColumn7Row204Neighbor];
  const FieldElementT& column7_row236 = neighbors[kColumn7Row236Neighbor];
  const FieldElementT& column7_row16343 = neighbors[kColumn7Row16343Neighbor];
  const FieldElementT& column7_row16351 = neighbors[kColumn7Row16351Neighbor];
  const FieldElementT& column7_row16367 = neighbors[kColumn7Row16367Neighbor];
  const FieldElementT& column7_row16375 = neighbors[kColumn7Row16375Neighbor];
  const FieldElementT& column7_row16383 = neighbors[kColumn7Row16383Neighbor];
  const FieldElementT& column7_row16391 = neighbors[kColumn7Row16391Neighbor];
  const FieldElementT& column7_row16423 = neighbors[kColumn7Row16423Neighbor];
  const FieldElementT& column7_row32727 = neighbors[kColumn7Row32727Neighbor];
  const FieldElementT& column7_row32735 = neighbors[kColumn7Row32735Neighbor];
  const FieldElementT& column7_row32759 = neighbors[kColumn7Row32759Neighbor];
  const FieldElementT& column7_row32767 = neighbors[kColumn7Row32767Neighbor];
  const FieldElementT& column8_row0 = neighbors[kColumn8Row0Neighbor];
  const FieldElementT& column8_row16 = neighbors[kColumn8Row16Neighbor];
  const FieldElementT& column8_row32 = neighbors[kColumn8Row32Neighbor];
  const FieldElementT& column8_row64 = neighbors[kColumn8Row64Neighbor];
  const FieldElementT& column8_row80 = neighbors[kColumn8Row80Neighbor];
  const FieldElementT& column8_row96 = neighbors[kColumn8Row96Neighbor];
  const FieldElementT& column8_row128 = neighbors[kColumn8Row128Neighbor];
  const FieldElementT& column8_row160 = neighbors[kColumn8Row160Neighbor];
  const FieldElementT& column8_row192 = neighbors[kColumn8Row192Neighbor];
  const FieldElementT& column8_row32640 = neighbors[kColumn8Row32640Neighbor];
  const FieldElementT& column8_row32656 = neighbors[kColumn8Row32656Neighbor];
  const FieldElementT& column8_row32704 = neighbors[kColumn8Row32704Neighbor];
  const FieldElementT& column8_row32736 = neighbors[kColumn8Row32736Neighbor];
  const FieldElementT& column9_inter1_row0 = neighbors[kColumn9Inter1Row0Neighbor];
  const FieldElementT& column9_inter1_row1 = neighbors[kColumn9Inter1Row1Neighbor];
  const FieldElementT& column9_inter1_row2 = neighbors[kColumn9Inter1Row2Neighbor];
  const FieldElementT& column9_inter1_row5 = neighbors[kColumn9Inter1Row5Neighbor];

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
      ((column5_row0) + (cpu__decode__opcode_rc__bit_2)) + (FieldElementT::One());
  const FieldElementT cpu__decode__opcode_rc__bit_10 =
      (column0_row10) - ((column0_row11) + (column0_row11));
  const FieldElementT cpu__decode__opcode_rc__bit_11 =
      (column0_row11) - ((column0_row12) + (column0_row12));
  const FieldElementT cpu__decode__opcode_rc__bit_14 =
      (column0_row14) - ((column0_row15) + (column0_row15));
  const FieldElementT memory__address_diff_0 = (column6_row2) - (column6_row0);
  const FieldElementT rc16__diff_0 = (column7_row6) - (column7_row2);
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_0 =
      (column3_row0) - ((column3_row1) + (column3_row1));
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash0__ec_subset_sum__bit_0);
  const FieldElementT rc_builtin__value0_0 = column7_row12;
  const FieldElementT rc_builtin__value1_0 =
      ((rc_builtin__value0_0) * (offset_size_)) + (column7_row44);
  const FieldElementT rc_builtin__value2_0 =
      ((rc_builtin__value1_0) * (offset_size_)) + (column7_row76);
  const FieldElementT rc_builtin__value3_0 =
      ((rc_builtin__value2_0) * (offset_size_)) + (column7_row108);
  const FieldElementT rc_builtin__value4_0 =
      ((rc_builtin__value3_0) * (offset_size_)) + (column7_row140);
  const FieldElementT rc_builtin__value5_0 =
      ((rc_builtin__value4_0) * (offset_size_)) + (column7_row172);
  const FieldElementT rc_builtin__value6_0 =
      ((rc_builtin__value5_0) * (offset_size_)) + (column7_row204);
  const FieldElementT rc_builtin__value7_0 =
      ((rc_builtin__value6_0) * (offset_size_)) + (column7_row236);
  const FieldElementT ecdsa__signature0__doubling_key__x_squared = (column7_row7) * (column7_row7);
  const FieldElementT ecdsa__signature0__exponentiate_generator__bit_0 =
      (column8_row32) - ((column8_row160) + (column8_row160));
  const FieldElementT ecdsa__signature0__exponentiate_generator__bit_neg_0 =
      (FieldElementT::One()) - (ecdsa__signature0__exponentiate_generator__bit_0);
  const FieldElementT ecdsa__signature0__exponentiate_key__bit_0 =
      (column7_row15) - ((column7_row79) + (column7_row79));
  const FieldElementT ecdsa__signature0__exponentiate_key__bit_neg_0 =
      (FieldElementT::One()) - (ecdsa__signature0__exponentiate_key__bit_0);
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
        inner_sum += random_coefficients[53] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/slope:
        const FieldElementT constraint =
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column2_row0) - (pedersen__points__y))) -
            ((column4_row0) * ((column1_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[56] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column4_row0) * (column4_row0)) -
            ((pedersen__hash0__ec_subset_sum__bit_0) *
             (((column1_row0) + (pedersen__points__x)) + (column1_row1)));
        inner_sum += random_coefficients[57] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column2_row0) + (column2_row1))) -
            ((column4_row0) * ((column1_row0) - (column1_row1)));
        inner_sum += random_coefficients[58] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column1_row1) - (column1_row0));
        inner_sum += random_coefficients[59] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column2_row1) - (column2_row0));
        inner_sum += random_coefficients[60] * constraint;
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
            (column5_row1) -
            (((((((column0_row0) * (offset_size_)) + (column7_row4)) * (offset_size_)) +
               (column7_row8)) *
              (offset_size_)) +
             (column7_row0));
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
            ((column5_row8) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_0) * (column7_row9)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_0)) * (column7_row1))) +
             (column7_row0));
        inner_sum += random_coefficients[7] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem0_addr:
        const FieldElementT constraint =
            ((column5_row4) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_1) * (column7_row9)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_1)) * (column7_row1))) +
             (column7_row8));
        inner_sum += random_coefficients[8] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem1_addr:
        const FieldElementT constraint = ((column5_row12) + (half_offset_size_)) -
                                         ((((((cpu__decode__opcode_rc__bit_2) * (column5_row0)) +
                                             ((cpu__decode__opcode_rc__bit_4) * (column7_row1))) +
                                            ((cpu__decode__opcode_rc__bit_3) * (column7_row9))) +
                                           ((cpu__decode__flag_op1_base_op0_0) * (column5_row5))) +
                                          (column7_row4));
        inner_sum += random_coefficients[9] * constraint;
      }
      {
        // Constraint expression for cpu/operands/ops_mul:
        const FieldElementT constraint = (column7_row5) - ((column5_row5) * (column5_row13));
        inner_sum += random_coefficients[10] * constraint;
      }
      {
        // Constraint expression for cpu/operands/res:
        const FieldElementT constraint =
            (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column7_row13)) -
            ((((cpu__decode__opcode_rc__bit_5) * ((column5_row5) + (column5_row13))) +
              ((cpu__decode__opcode_rc__bit_6) * (column7_row5))) +
             ((cpu__decode__flag_res_op1_0) * (column5_row13)));
        inner_sum += random_coefficients[11] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_fp:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column5_row9) - (column7_row9));
        inner_sum += random_coefficients[18] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_pc:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) *
            ((column5_row5) -
             (((column5_row0) + (cpu__decode__opcode_rc__bit_2)) + (FieldElementT::One())));
        inner_sum += random_coefficients[19] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off0:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column7_row0) - (half_offset_size_));
        inner_sum += random_coefficients[20] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off1:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) *
            ((column7_row8) - ((half_offset_size_) + (FieldElementT::One())));
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
            (((column7_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z))) - (half_offset_size_));
        inner_sum += random_coefficients[23] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/off2:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_13) *
            (((column7_row4) + (FieldElementT::One())) - (half_offset_size_));
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
            (cpu__decode__opcode_rc__bit_14) * ((column5_row9) - (column7_row13));
        inner_sum += random_coefficients[26] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain19.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp0:
        const FieldElementT constraint =
            (column7_row3) - ((cpu__decode__opcode_rc__bit_9) * (column5_row9));
        inner_sum += random_coefficients[12] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp1:
        const FieldElementT constraint = (column7_row11) - ((column7_row3) * (column7_row13));
        inner_sum += random_coefficients[13] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_negative:
        const FieldElementT constraint =
            ((((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column5_row16)) +
             ((column7_row3) * ((column5_row16) - ((column5_row0) + (column5_row13))))) -
            ((((cpu__decode__flag_pc_update_regular_0) * (npc_reg_0)) +
              ((cpu__decode__opcode_rc__bit_7) * (column7_row13))) +
             ((cpu__decode__opcode_rc__bit_8) * ((column5_row0) + (column7_row13))));
        inner_sum += random_coefficients[14] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_positive:
        const FieldElementT constraint =
            ((column7_row11) - (cpu__decode__opcode_rc__bit_9)) * ((column5_row16) - (npc_reg_0));
        inner_sum += random_coefficients[15] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_ap/ap_update:
        const FieldElementT constraint =
            (column7_row17) -
            ((((column7_row1) + ((cpu__decode__opcode_rc__bit_10) * (column7_row13))) +
              (cpu__decode__opcode_rc__bit_11)) +
             ((cpu__decode__opcode_rc__bit_12) * (FieldElementT::ConstexprFromBigInt(0x2_Z))));
        inner_sum += random_coefficients[16] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_fp/fp_update:
        const FieldElementT constraint =
            (column7_row25) - ((((cpu__decode__fp_update_regular_0) * (column7_row9)) +
                                ((cpu__decode__opcode_rc__bit_13) * (column5_row9))) +
                               ((cpu__decode__opcode_rc__bit_12) *
                                ((column7_row1) + (FieldElementT::ConstexprFromBigInt(0x2_Z)))));
        inner_sum += random_coefficients[17] * constraint;
      }
      outer_sum += inner_sum * domain19;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain5);
  }

  {
    // Compute a sum of constraints with denominator = domain20.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for initial_ap:
        const FieldElementT constraint = (column7_row1) - (initial_ap_);
        inner_sum += random_coefficients[27] * constraint;
      }
      {
        // Constraint expression for initial_fp:
        const FieldElementT constraint = (column7_row9) - (initial_ap_);
        inner_sum += random_coefficients[28] * constraint;
      }
      {
        // Constraint expression for initial_pc:
        const FieldElementT constraint = (column5_row0) - (initial_pc_);
        inner_sum += random_coefficients[29] * constraint;
      }
      {
        // Constraint expression for memory/multi_column_perm/perm/init0:
        const FieldElementT constraint =
            (((((memory__multi_column_perm__perm__interaction_elm_) -
                ((column6_row0) +
                 ((memory__multi_column_perm__hash_interaction_elm0_) * (column6_row1)))) *
               (column9_inter1_row0)) +
              (column5_row0)) +
             ((memory__multi_column_perm__hash_interaction_elm0_) * (column5_row1))) -
            (memory__multi_column_perm__perm__interaction_elm_);
        inner_sum += random_coefficients[33] * constraint;
      }
      {
        // Constraint expression for memory/initial_addr:
        const FieldElementT constraint = (column6_row0) - (FieldElementT::One());
        inner_sum += random_coefficients[38] * constraint;
      }
      {
        // Constraint expression for rc16/perm/init0:
        const FieldElementT constraint =
            ((((rc16__perm__interaction_elm_) - (column7_row2)) * (column9_inter1_row1)) +
             (column7_row0)) -
            (rc16__perm__interaction_elm_);
        inner_sum += random_coefficients[41] * constraint;
      }
      {
        // Constraint expression for rc16/minimum:
        const FieldElementT constraint = (column7_row2) - (rc_min_);
        inner_sum += random_coefficients[45] * constraint;
      }
      {
        // Constraint expression for pedersen/init_addr:
        const FieldElementT constraint = (column5_row6) - (initial_pedersen_addr_);
        inner_sum += random_coefficients[67] * constraint;
      }
      {
        // Constraint expression for rc_builtin/init_addr:
        const FieldElementT constraint = (column5_row70) - (initial_rc_addr_);
        inner_sum += random_coefficients[74] * constraint;
      }
      {
        // Constraint expression for ecdsa/init_addr:
        const FieldElementT constraint = (column5_row390) - (initial_ecdsa_addr_);
        inner_sum += random_coefficients[111] * constraint;
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
        // Constraint expression for final_ap:
        const FieldElementT constraint = (column7_row1) - (final_ap_);
        inner_sum += random_coefficients[30] * constraint;
      }
      {
        // Constraint expression for final_fp:
        const FieldElementT constraint = (column7_row9) - (initial_ap_);
        inner_sum += random_coefficients[31] * constraint;
      }
      {
        // Constraint expression for final_pc:
        const FieldElementT constraint = (column5_row0) - (final_pc_);
        inner_sum += random_coefficients[32] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain19);
  }

  {
    // Compute a sum of constraints with denominator = domain1.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain21.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/step0:
        const FieldElementT constraint =
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column6_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column6_row3)))) *
             (column9_inter1_row2)) -
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column5_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column5_row3)))) *
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
            ((memory__address_diff_0) - (FieldElementT::One())) * ((column6_row1) - (column6_row3));
        inner_sum += random_coefficients[37] * constraint;
      }
      outer_sum += inner_sum * domain21;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain1);
  }

  {
    // Compute a sum of constraints with denominator = domain21.
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
    res += FractionFieldElement<FieldElementT>(outer_sum, domain21);
  }

  {
    // Compute a sum of constraints with denominator = domain3.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for public_memory_addr_zero:
        const FieldElementT constraint = column5_row2;
        inner_sum += random_coefficients[39] * constraint;
      }
      {
        // Constraint expression for public_memory_value_zero:
        const FieldElementT constraint = column5_row3;
        inner_sum += random_coefficients[40] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain3);
  }

  {
    // Compute a sum of constraints with denominator = domain2.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain22.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc16/perm/step0:
        const FieldElementT constraint =
            (((rc16__perm__interaction_elm_) - (column7_row6)) * (column9_inter1_row5)) -
            (((rc16__perm__interaction_elm_) - (column7_row4)) * (column9_inter1_row1));
        inner_sum += random_coefficients[42] * constraint;
      }
      {
        // Constraint expression for rc16/diff_is_bit:
        const FieldElementT constraint = ((rc16__diff_0) * (rc16__diff_0)) - (rc16__diff_0);
        inner_sum += random_coefficients[44] * constraint;
      }
      outer_sum += inner_sum * domain22;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain2);
  }

  {
    // Compute a sum of constraints with denominator = domain22.
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
        const FieldElementT constraint = (column7_row2) - (rc_max_);
        inner_sum += random_coefficients[46] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain22);
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
            (column8_row80) * ((column3_row0) - ((column3_row1) + (column3_row1)));
        inner_sum += random_coefficients[47] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column8_row80) *
            ((column3_row1) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column3_row192)));
        inner_sum += random_coefficients[48] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column8_row80) -
            ((column4_row255) * ((column3_row192) - ((column3_row193) + (column3_row193))));
        inner_sum += random_coefficients[49] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column4_row255) *
            ((column3_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column3_row196)));
        inner_sum += random_coefficients[50] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column4_row255) - (((column3_row251) - ((column3_row252) + (column3_row252))) *
                                ((column3_row196) - ((column3_row197) + (column3_row197))));
        inner_sum += random_coefficients[51] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column3_row251) - ((column3_row252) + (column3_row252))) *
            ((column3_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column3_row251)));
        inner_sum += random_coefficients[52] * constraint;
      }
      {
        // Constraint expression for rc_builtin/value:
        const FieldElementT constraint = (rc_builtin__value7_0) - (column5_row71);
        inner_sum += random_coefficients[72] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain11.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/copy_point/x:
        const FieldElementT constraint = (column1_row256) - (column1_row255);
        inner_sum += random_coefficients[61] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/copy_point/y:
        const FieldElementT constraint = (column2_row256) - (column2_row255);
        inner_sum += random_coefficients[62] * constraint;
      }
      outer_sum += inner_sum * domain11;
    }

    {
      // Compute a sum of constraints with numerator = domain23.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc_builtin/addr_step:
        const FieldElementT constraint =
            (column5_row326) - ((column5_row70) + (FieldElementT::One()));
        inner_sum += random_coefficients[73] * constraint;
      }
      outer_sum += inner_sum * domain23;
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
        inner_sum += random_coefficients[54] * constraint;
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
        inner_sum += random_coefficients[55] * constraint;
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
        inner_sum += random_coefficients[63] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/init/y:
        const FieldElementT constraint = (column2_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[64] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value0:
        const FieldElementT constraint = (column5_row7) - (column3_row0);
        inner_sum += random_coefficients[65] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value0:
        const FieldElementT constraint = (column5_row263) - (column3_row256);
        inner_sum += random_coefficients[68] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_addr:
        const FieldElementT constraint =
            (column5_row262) - ((column5_row6) + (FieldElementT::One()));
        inner_sum += random_coefficients[69] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value0:
        const FieldElementT constraint = (column5_row135) - (column1_row511);
        inner_sum += random_coefficients[70] * constraint;
      }
      {
        // Constraint expression for pedersen/output_addr:
        const FieldElementT constraint =
            (column5_row134) - ((column5_row262) + (FieldElementT::One()));
        inner_sum += random_coefficients[71] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain24.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input0_addr:
        const FieldElementT constraint =
            (column5_row518) - ((column5_row134) + (FieldElementT::One()));
        inner_sum += random_coefficients[66] * constraint;
      }
      outer_sum += inner_sum * domain24;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain12);
  }

  {
    // Compute a sum of constraints with denominator = domain6.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain13.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/doubling_key/slope:
        const FieldElementT constraint = ((((ecdsa__signature0__doubling_key__x_squared) +
                                            (ecdsa__signature0__doubling_key__x_squared)) +
                                           (ecdsa__signature0__doubling_key__x_squared)) +
                                          ((ecdsa__sig_config_).alpha)) -
                                         (((column7_row39) + (column7_row39)) * (column7_row47));
        inner_sum += random_coefficients[75] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/doubling_key/x:
        const FieldElementT constraint = ((column7_row47) * (column7_row47)) -
                                         (((column7_row7) + (column7_row7)) + (column7_row71));
        inner_sum += random_coefficients[76] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/doubling_key/y:
        const FieldElementT constraint = ((column7_row39) + (column7_row103)) -
                                         ((column7_row47) * ((column7_row7) - (column7_row71)));
        inner_sum += random_coefficients[77] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/booleanity_test:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_0) *
            ((ecdsa__signature0__exponentiate_key__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[87] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/slope:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_key__bit_0) * ((column7_row55) - (column7_row39))) -
            ((column7_row31) * ((column7_row23) - (column7_row7)));
        inner_sum += random_coefficients[90] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/x:
        const FieldElementT constraint = ((column7_row31) * (column7_row31)) -
                                         ((ecdsa__signature0__exponentiate_key__bit_0) *
                                          (((column7_row23) + (column7_row7)) + (column7_row87)));
        inner_sum += random_coefficients[91] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/y:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_key__bit_0) * ((column7_row55) + (column7_row119))) -
            ((column7_row31) * ((column7_row23) - (column7_row87)));
        inner_sum += random_coefficients[92] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column7_row63) * ((column7_row23) - (column7_row7))) - (FieldElementT::One());
        inner_sum += random_coefficients[93] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/copy_point/x:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_neg_0) * ((column7_row87) - (column7_row23));
        inner_sum += random_coefficients[94] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/copy_point/y:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_neg_0) * ((column7_row119) - (column7_row55));
        inner_sum += random_coefficients[95] * constraint;
      }
      outer_sum += inner_sum * domain13;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain6);
  }

  {
    // Compute a sum of constraints with denominator = domain7.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain16.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/booleanity_test:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_generator__bit_0) *
            ((ecdsa__signature0__exponentiate_generator__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[78] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/slope:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_generator__bit_0) *
             ((column8_row64) - (ecdsa__generator_points__y))) -
            ((column8_row96) * ((column8_row0) - (ecdsa__generator_points__x)));
        inner_sum += random_coefficients[81] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/x:
        const FieldElementT constraint =
            ((column8_row96) * (column8_row96)) -
            ((ecdsa__signature0__exponentiate_generator__bit_0) *
             (((column8_row0) + (ecdsa__generator_points__x)) + (column8_row128)));
        inner_sum += random_coefficients[82] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/y:
        const FieldElementT constraint = ((ecdsa__signature0__exponentiate_generator__bit_0) *
                                          ((column8_row64) + (column8_row192))) -
                                         ((column8_row96) * ((column8_row0) - (column8_row128)));
        inner_sum += random_coefficients[83] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column8_row16) * ((column8_row0) - (ecdsa__generator_points__x))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[84] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/copy_point/x:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_generator__bit_neg_0) *
                                         ((column8_row128) - (column8_row0));
        inner_sum += random_coefficients[85] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/copy_point/y:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_generator__bit_neg_0) *
                                         ((column8_row192) - (column8_row64));
        inner_sum += random_coefficients[86] * constraint;
      }
      outer_sum += inner_sum * domain16;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain7);
  }

  {
    // Compute a sum of constraints with denominator = domain17.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/bit_extraction_end:
        const FieldElementT constraint = column8_row32;
        inner_sum += random_coefficients[79] * constraint;
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
        // Constraint expression for ecdsa/signature0/exponentiate_generator/zeros_tail:
        const FieldElementT constraint = column8_row32;
        inner_sum += random_coefficients[80] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain16);
  }

  {
    // Compute a sum of constraints with denominator = domain14.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/bit_extraction_end:
        const FieldElementT constraint = column7_row15;
        inner_sum += random_coefficients[88] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain14);
  }

  {
    // Compute a sum of constraints with denominator = domain13.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/zeros_tail:
        const FieldElementT constraint = column7_row15;
        inner_sum += random_coefficients[89] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain13);
  }

  {
    // Compute a sum of constraints with denominator = domain18.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/init_gen/x:
        const FieldElementT constraint = (column8_row0) - (((ecdsa__sig_config_).shift_point).x);
        inner_sum += random_coefficients[96] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/init_gen/y:
        const FieldElementT constraint = (column8_row64) + (((ecdsa__sig_config_).shift_point).y);
        inner_sum += random_coefficients[97] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/slope:
        const FieldElementT constraint =
            (column8_row32704) -
            ((column7_row16375) + ((column8_row32736) * ((column8_row32640) - (column7_row16343))));
        inner_sum += random_coefficients[100] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/x:
        const FieldElementT constraint =
            ((column8_row32736) * (column8_row32736)) -
            (((column8_row32640) + (column7_row16343)) + (column7_row16391));
        inner_sum += random_coefficients[101] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/y:
        const FieldElementT constraint =
            ((column8_row32704) + (column7_row16423)) -
            ((column8_row32736) * ((column8_row32640) - (column7_row16391)));
        inner_sum += random_coefficients[102] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/x_diff_inv:
        const FieldElementT constraint =
            ((column8_row32656) * ((column8_row32640) - (column7_row16343))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[103] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/slope:
        const FieldElementT constraint =
            ((column7_row32759) + (((ecdsa__sig_config_).shift_point).y)) -
            ((column7_row16351) * ((column7_row32727) - (((ecdsa__sig_config_).shift_point).x)));
        inner_sum += random_coefficients[104] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/x:
        const FieldElementT constraint =
            ((column7_row16351) * (column7_row16351)) -
            (((column7_row32727) + (((ecdsa__sig_config_).shift_point).x)) + (column7_row15));
        inner_sum += random_coefficients[105] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/x_diff_inv:
        const FieldElementT constraint =
            ((column7_row32735) * ((column7_row32727) - (((ecdsa__sig_config_).shift_point).x))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[106] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/z_nonzero:
        const FieldElementT constraint =
            ((column8_row32) * (column7_row16383)) - (FieldElementT::One());
        inner_sum += random_coefficients[107] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/q_on_curve/x_squared:
        const FieldElementT constraint = (column7_row32767) - ((column7_row7) * (column7_row7));
        inner_sum += random_coefficients[109] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/q_on_curve/on_curve:
        const FieldElementT constraint = ((column7_row39) * (column7_row39)) -
                                         ((((column7_row7) * (column7_row32767)) +
                                           (((ecdsa__sig_config_).alpha) * (column7_row7))) +
                                          ((ecdsa__sig_config_).beta));
        inner_sum += random_coefficients[110] * constraint;
      }
      {
        // Constraint expression for ecdsa/message_addr:
        const FieldElementT constraint =
            (column5_row16774) - ((column5_row390) + (FieldElementT::One()));
        inner_sum += random_coefficients[112] * constraint;
      }
      {
        // Constraint expression for ecdsa/message_value0:
        const FieldElementT constraint = (column5_row16775) - (column8_row32);
        inner_sum += random_coefficients[114] * constraint;
      }
      {
        // Constraint expression for ecdsa/pubkey_value0:
        const FieldElementT constraint = (column5_row391) - (column7_row7);
        inner_sum += random_coefficients[115] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain25.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/pubkey_addr:
        const FieldElementT constraint =
            (column5_row33158) - ((column5_row16774) + (FieldElementT::One()));
        inner_sum += random_coefficients[113] * constraint;
      }
      outer_sum += inner_sum * domain25;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain18);
  }

  {
    // Compute a sum of constraints with denominator = domain15.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/init_key/x:
        const FieldElementT constraint = (column7_row23) - (((ecdsa__sig_config_).shift_point).x);
        inner_sum += random_coefficients[98] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/init_key/y:
        const FieldElementT constraint = (column7_row55) - (((ecdsa__sig_config_).shift_point).y);
        inner_sum += random_coefficients[99] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/r_and_w_nonzero:
        const FieldElementT constraint =
            ((column7_row15) * (column7_row16367)) - (FieldElementT::One());
        inner_sum += random_coefficients[108] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain15);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> CpuAirDefinition<FieldElementT, 2>::DomainEvalsAtPoint(
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
  const FieldElementT& domain13 = (point_powers[10]) - (shifts[1]);
  const FieldElementT& domain14 = (point_powers[10]) - (shifts[4]);
  const FieldElementT& domain15 = (point_powers[10]) - (FieldElementT::One());
  const FieldElementT& domain16 = (point_powers[11]) - (shifts[1]);
  const FieldElementT& domain17 = (point_powers[11]) - (shifts[4]);
  const FieldElementT& domain18 = (point_powers[11]) - (FieldElementT::One());
  return {
      domain0,  domain1,  domain2,  domain3,  domain4,  domain5,  domain6,
      domain7,  domain8,  domain9,  domain10, domain11, domain12, domain13,
      domain14, domain15, domain16, domain17, domain18,
  };
}

template <typename FieldElementT>
TraceGenerationContext CpuAirDefinition<FieldElementT, 2>::GetTraceGenerationContext() const {
  TraceGenerationContext ctx;

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 32768)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 32768)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 32768)) <= (SafeDiv(trace_length_, 32768)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 32768)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 32768)), "Index out of range.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 32768)) - (1)) <= (SafeDiv(trace_length_, 32768)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 32768)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 32768)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 32768)), "Index out of range.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 256)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 256)), "Index out of range.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 256)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 256)) - (1)) <= (SafeDiv(trace_length_, 256)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 256)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 256)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 256)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 256)) <= (SafeDiv(trace_length_, 256)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 256)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 512)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 512)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 512)) <= (SafeDiv(trace_length_, 512)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 512)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 512)), "Index out of range.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 512)), "Index out of range.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 512)) - (1)) <= (SafeDiv(trace_length_, 512)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 512)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 512)) - (1)), "start must not exceed stop.");

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
      "pedersen/hash0/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "mem_pool/addr", VirtualColumn(/*column=*/kColumn5Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "mem_pool/value", VirtualColumn(/*column=*/kColumn5Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "memory/sorted/addr", VirtualColumn(/*column=*/kColumn6Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/sorted/value",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "rc16_pool", VirtualColumn(/*column=*/kColumn7Column, /*step=*/4, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "rc16/sorted", VirtualColumn(/*column=*/kColumn7Column, /*step=*/4, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "cpu/registers/ap", VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/registers/fp", VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "cpu/operands/ops_mul",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "cpu/operands/res", VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp0",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp1",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/11));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/key_points/x",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/64, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/key_points/y",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/64, /*row_offset=*/39));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/partial_sum/x",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/64, /*row_offset=*/23));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/partial_sum/y",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/64, /*row_offset=*/55));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/selector",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/64, /*row_offset=*/15));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/doubling_slope",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/64, /*row_offset=*/47));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/slope",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/64, /*row_offset=*/31));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/x_diff_inv",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/64, /*row_offset=*/63));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/partial_sum/x",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/128, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/partial_sum/y",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/128, /*row_offset=*/64));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/selector",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/128, /*row_offset=*/32));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/slope",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/128, /*row_offset=*/96));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/x_diff_inv",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/128, /*row_offset=*/16));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/256, /*row_offset=*/255));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/256, /*row_offset=*/80));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/r_w_inv",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/16384, /*row_offset=*/16367));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/add_results_slope",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/32768, /*row_offset=*/32736));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/add_results_inv",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/32768, /*row_offset=*/32656));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/extract_r_slope",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/32768, /*row_offset=*/16351));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/extract_r_inv",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/32768, /*row_offset=*/32735));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/z_inv",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/32768, /*row_offset=*/16383));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/q_x_squared",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/32768, /*row_offset=*/32767));
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
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/mem_inst/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/pc", VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/instruction",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/off0", VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/off1", VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/decode/off2", VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_dst/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_dst/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op0/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op0/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op1/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op1/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "orig/public_memory/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/8, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "orig/public_memory/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/8, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "pedersen/input0/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/512, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "pedersen/input0/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/512, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "pedersen/input1/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/512, /*row_offset=*/262));
  ctx.AddVirtualColumn(
      "pedersen/input1/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/512, /*row_offset=*/263));
  ctx.AddVirtualColumn(
      "pedersen/output/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/512, /*row_offset=*/134));
  ctx.AddVirtualColumn(
      "pedersen/output/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/512, /*row_offset=*/135));
  ctx.AddVirtualColumn(
      "rc_builtin/mem/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/256, /*row_offset=*/70));
  ctx.AddVirtualColumn(
      "rc_builtin/mem/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/256, /*row_offset=*/71));
  ctx.AddVirtualColumn(
      "rc_builtin/inner_rc",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/32, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "ecdsa/pubkey/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/32768, /*row_offset=*/390));
  ctx.AddVirtualColumn(
      "ecdsa/pubkey/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/32768, /*row_offset=*/391));
  ctx.AddVirtualColumn(
      "ecdsa/message/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/32768, /*row_offset=*/16774));
  ctx.AddVirtualColumn(
      "ecdsa/message/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/32768, /*row_offset=*/16775));

  ctx.AddPeriodicColumn(
      "pedersen/points/x",
      VirtualColumn(/*column=*/kPedersenPointsXPeriodicColumn, /*step=*/1, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "pedersen/points/y",
      VirtualColumn(/*column=*/kPedersenPointsYPeriodicColumn, /*step=*/1, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "ecdsa/generator_points/x",
      VirtualColumn(
          /*column=*/kEcdsaGeneratorPointsXPeriodicColumn, /*step=*/128, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "ecdsa/generator_points/y",
      VirtualColumn(
          /*column=*/kEcdsaGeneratorPointsYPeriodicColumn, /*step=*/128, /*row_offset=*/0));

  ctx.AddObject<std::vector<size_t>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);

  return ctx;
}

template <typename FieldElementT>
std::vector<std::pair<int64_t, uint64_t>> CpuAirDefinition<FieldElementT, 2>::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;

  mask.reserve(128);
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
  mask.emplace_back(255, kColumn4Column);
  mask.emplace_back(0, kColumn5Column);
  mask.emplace_back(1, kColumn5Column);
  mask.emplace_back(2, kColumn5Column);
  mask.emplace_back(3, kColumn5Column);
  mask.emplace_back(4, kColumn5Column);
  mask.emplace_back(5, kColumn5Column);
  mask.emplace_back(6, kColumn5Column);
  mask.emplace_back(7, kColumn5Column);
  mask.emplace_back(8, kColumn5Column);
  mask.emplace_back(9, kColumn5Column);
  mask.emplace_back(12, kColumn5Column);
  mask.emplace_back(13, kColumn5Column);
  mask.emplace_back(16, kColumn5Column);
  mask.emplace_back(70, kColumn5Column);
  mask.emplace_back(71, kColumn5Column);
  mask.emplace_back(134, kColumn5Column);
  mask.emplace_back(135, kColumn5Column);
  mask.emplace_back(262, kColumn5Column);
  mask.emplace_back(263, kColumn5Column);
  mask.emplace_back(326, kColumn5Column);
  mask.emplace_back(390, kColumn5Column);
  mask.emplace_back(391, kColumn5Column);
  mask.emplace_back(518, kColumn5Column);
  mask.emplace_back(16774, kColumn5Column);
  mask.emplace_back(16775, kColumn5Column);
  mask.emplace_back(33158, kColumn5Column);
  mask.emplace_back(0, kColumn6Column);
  mask.emplace_back(1, kColumn6Column);
  mask.emplace_back(2, kColumn6Column);
  mask.emplace_back(3, kColumn6Column);
  mask.emplace_back(0, kColumn7Column);
  mask.emplace_back(1, kColumn7Column);
  mask.emplace_back(2, kColumn7Column);
  mask.emplace_back(3, kColumn7Column);
  mask.emplace_back(4, kColumn7Column);
  mask.emplace_back(5, kColumn7Column);
  mask.emplace_back(6, kColumn7Column);
  mask.emplace_back(7, kColumn7Column);
  mask.emplace_back(8, kColumn7Column);
  mask.emplace_back(9, kColumn7Column);
  mask.emplace_back(11, kColumn7Column);
  mask.emplace_back(12, kColumn7Column);
  mask.emplace_back(13, kColumn7Column);
  mask.emplace_back(15, kColumn7Column);
  mask.emplace_back(17, kColumn7Column);
  mask.emplace_back(23, kColumn7Column);
  mask.emplace_back(25, kColumn7Column);
  mask.emplace_back(31, kColumn7Column);
  mask.emplace_back(39, kColumn7Column);
  mask.emplace_back(44, kColumn7Column);
  mask.emplace_back(47, kColumn7Column);
  mask.emplace_back(55, kColumn7Column);
  mask.emplace_back(63, kColumn7Column);
  mask.emplace_back(71, kColumn7Column);
  mask.emplace_back(76, kColumn7Column);
  mask.emplace_back(79, kColumn7Column);
  mask.emplace_back(87, kColumn7Column);
  mask.emplace_back(103, kColumn7Column);
  mask.emplace_back(108, kColumn7Column);
  mask.emplace_back(119, kColumn7Column);
  mask.emplace_back(140, kColumn7Column);
  mask.emplace_back(172, kColumn7Column);
  mask.emplace_back(204, kColumn7Column);
  mask.emplace_back(236, kColumn7Column);
  mask.emplace_back(16343, kColumn7Column);
  mask.emplace_back(16351, kColumn7Column);
  mask.emplace_back(16367, kColumn7Column);
  mask.emplace_back(16375, kColumn7Column);
  mask.emplace_back(16383, kColumn7Column);
  mask.emplace_back(16391, kColumn7Column);
  mask.emplace_back(16423, kColumn7Column);
  mask.emplace_back(32727, kColumn7Column);
  mask.emplace_back(32735, kColumn7Column);
  mask.emplace_back(32759, kColumn7Column);
  mask.emplace_back(32767, kColumn7Column);
  mask.emplace_back(0, kColumn8Column);
  mask.emplace_back(16, kColumn8Column);
  mask.emplace_back(32, kColumn8Column);
  mask.emplace_back(64, kColumn8Column);
  mask.emplace_back(80, kColumn8Column);
  mask.emplace_back(96, kColumn8Column);
  mask.emplace_back(128, kColumn8Column);
  mask.emplace_back(160, kColumn8Column);
  mask.emplace_back(192, kColumn8Column);
  mask.emplace_back(32640, kColumn8Column);
  mask.emplace_back(32656, kColumn8Column);
  mask.emplace_back(32704, kColumn8Column);
  mask.emplace_back(32736, kColumn8Column);
  mask.emplace_back(0, kColumn9Inter1Column);
  mask.emplace_back(1, kColumn9Inter1Column);
  mask.emplace_back(2, kColumn9Inter1Column);
  mask.emplace_back(5, kColumn9Inter1Column);

  return mask;
}

}  // namespace cpu

}  // namespace starkware
