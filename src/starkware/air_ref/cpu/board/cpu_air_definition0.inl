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
CpuAirDefinition<FieldElementT, 0>::CreateCompositionPolynomial(
    const FieldElement& trace_generator, const ConstFieldElementSpan& random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const FieldElementT& gen = trace_generator.As<FieldElementT>();

  const std::vector<uint64_t> point_exponents = {
      trace_length_,
      SafeDiv(trace_length_, 2),
      SafeDiv(trace_length_, 8),
      SafeDiv(trace_length_, 16),
      SafeDiv(trace_length_, 32),
      SafeDiv(trace_length_, 128),
      SafeDiv(trace_length_, 256),
      SafeDiv(trace_length_, 512),
      SafeDiv(trace_length_, 4096),
      SafeDiv(trace_length_, 8192)};
  const std::vector<uint64_t> gen_exponents = {SafeDiv((15) * (trace_length_), 16),
                                               SafeDiv((255) * (trace_length_), 256),
                                               SafeDiv((63) * (trace_length_), 64),
                                               SafeDiv(trace_length_, 2),
                                               SafeDiv((251) * (trace_length_), 256),
                                               (trace_length_) - (1),
                                               (16) * ((SafeDiv(trace_length_, 16)) - (1)),
                                               (2) * ((SafeDiv(trace_length_, 2)) - (1)),
                                               (128) * ((SafeDiv(trace_length_, 128)) - (1)),
                                               (8192) * ((SafeDiv(trace_length_, 8192)) - (1))};

  BuildPeriodicColumns(gen, &builder);

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
CpuAirDefinition<FieldElementT, 0>::PrecomputeDomainEvalsOnCoset(
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
      FieldElementT::UninitializedVector(8),    FieldElementT::UninitializedVector(16),
      FieldElementT::UninitializedVector(16),   FieldElementT::UninitializedVector(32),
      FieldElementT::UninitializedVector(128),  FieldElementT::UninitializedVector(256),
      FieldElementT::UninitializedVector(256),  FieldElementT::UninitializedVector(256),
      FieldElementT::UninitializedVector(512),  FieldElementT::UninitializedVector(512),
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

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[7][i] = (point_powers[6][i & (255)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[8][i] = (point_powers[6][i & (255)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[9][i] = (point_powers[6][i & (255)]) - (shifts[2]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[10][i] = (point_powers[7][i & (511)]) - (shifts[3]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[11][i] = (point_powers[7][i & (511)]) - (FieldElementT::One());
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

  period = 8192;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[15][i] = (point_powers[9][i & (8191)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 8192;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[16][i] = (point_powers[9][i & (8191)]) - (shifts[4]);
        }
      },
      period, kTaskSize);

  period = 8192;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[17][i] = (point_powers[9][i & (8191)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);
  return precomp_domains;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> CpuAirDefinition<FieldElementT, 0>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 10, "shifts should contain 10 elements.");

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
  // domain5 = point^(trace_length / 32) - 1.
  const FieldElementT& domain5 = precomp_domains[5];
  // domain6 = point^(trace_length / 128) - 1.
  const FieldElementT& domain6 = precomp_domains[6];
  // domain7 = point^(trace_length / 256) - gen^(255 * trace_length / 256).
  const FieldElementT& domain7 = precomp_domains[7];
  // domain8 = point^(trace_length / 256) - 1.
  const FieldElementT& domain8 = precomp_domains[8];
  // domain9 = point^(trace_length / 256) - gen^(63 * trace_length / 64).
  const FieldElementT& domain9 = precomp_domains[9];
  // domain10 = point^(trace_length / 512) - gen^(trace_length / 2).
  const FieldElementT& domain10 = precomp_domains[10];
  // domain11 = point^(trace_length / 512) - 1.
  const FieldElementT& domain11 = precomp_domains[11];
  // domain12 = point^(trace_length / 4096) - gen^(255 * trace_length / 256).
  const FieldElementT& domain12 = precomp_domains[12];
  // domain13 = point^(trace_length / 4096) - gen^(251 * trace_length / 256).
  const FieldElementT& domain13 = precomp_domains[13];
  // domain14 = point^(trace_length / 4096) - 1.
  const FieldElementT& domain14 = precomp_domains[14];
  // domain15 = point^(trace_length / 8192) - gen^(255 * trace_length / 256).
  const FieldElementT& domain15 = precomp_domains[15];
  // domain16 = point^(trace_length / 8192) - gen^(251 * trace_length / 256).
  const FieldElementT& domain16 = precomp_domains[16];
  // domain17 = point^(trace_length / 8192) - 1.
  const FieldElementT& domain17 = precomp_domains[17];
  // domain18 = point - gen^(trace_length - 1).
  const FieldElementT& domain18 = (point) - (shifts[5]);
  // domain19 = point - gen^(16 * (trace_length / 16 - 1)).
  const FieldElementT& domain19 = (point) - (shifts[6]);
  // domain20 = point - 1.
  const FieldElementT& domain20 = (point) - (FieldElementT::One());
  // domain21 = point - gen^(2 * (trace_length / 2 - 1)).
  const FieldElementT& domain21 = (point) - (shifts[7]);
  // domain22 = point - gen^(128 * (trace_length / 128 - 1)).
  const FieldElementT& domain22 = (point) - (shifts[8]);
  // domain23 = point - gen^(8192 * (trace_length / 8192 - 1)).
  const FieldElementT& domain23 = (point) - (shifts[9]);

  ASSERT_VERIFIER(neighbors.size() == 201, "Neighbors must contain 201 elements.");
  const FieldElementT& column0_row0 = neighbors[kColumn0Row0Neighbor];
  const FieldElementT& column0_row1 = neighbors[kColumn0Row1Neighbor];
  const FieldElementT& column0_row4 = neighbors[kColumn0Row4Neighbor];
  const FieldElementT& column0_row8 = neighbors[kColumn0Row8Neighbor];
  const FieldElementT& column0_row12 = neighbors[kColumn0Row12Neighbor];
  const FieldElementT& column0_row28 = neighbors[kColumn0Row28Neighbor];
  const FieldElementT& column0_row44 = neighbors[kColumn0Row44Neighbor];
  const FieldElementT& column0_row60 = neighbors[kColumn0Row60Neighbor];
  const FieldElementT& column0_row76 = neighbors[kColumn0Row76Neighbor];
  const FieldElementT& column0_row92 = neighbors[kColumn0Row92Neighbor];
  const FieldElementT& column0_row108 = neighbors[kColumn0Row108Neighbor];
  const FieldElementT& column0_row124 = neighbors[kColumn0Row124Neighbor];
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
  const FieldElementT& column19_row6 = neighbors[kColumn19Row6Neighbor];
  const FieldElementT& column19_row7 = neighbors[kColumn19Row7Neighbor];
  const FieldElementT& column19_row8 = neighbors[kColumn19Row8Neighbor];
  const FieldElementT& column19_row9 = neighbors[kColumn19Row9Neighbor];
  const FieldElementT& column19_row12 = neighbors[kColumn19Row12Neighbor];
  const FieldElementT& column19_row13 = neighbors[kColumn19Row13Neighbor];
  const FieldElementT& column19_row16 = neighbors[kColumn19Row16Neighbor];
  const FieldElementT& column19_row22 = neighbors[kColumn19Row22Neighbor];
  const FieldElementT& column19_row23 = neighbors[kColumn19Row23Neighbor];
  const FieldElementT& column19_row38 = neighbors[kColumn19Row38Neighbor];
  const FieldElementT& column19_row39 = neighbors[kColumn19Row39Neighbor];
  const FieldElementT& column19_row70 = neighbors[kColumn19Row70Neighbor];
  const FieldElementT& column19_row71 = neighbors[kColumn19Row71Neighbor];
  const FieldElementT& column19_row102 = neighbors[kColumn19Row102Neighbor];
  const FieldElementT& column19_row103 = neighbors[kColumn19Row103Neighbor];
  const FieldElementT& column19_row134 = neighbors[kColumn19Row134Neighbor];
  const FieldElementT& column19_row135 = neighbors[kColumn19Row135Neighbor];
  const FieldElementT& column19_row167 = neighbors[kColumn19Row167Neighbor];
  const FieldElementT& column19_row199 = neighbors[kColumn19Row199Neighbor];
  const FieldElementT& column19_row230 = neighbors[kColumn19Row230Neighbor];
  const FieldElementT& column19_row263 = neighbors[kColumn19Row263Neighbor];
  const FieldElementT& column19_row295 = neighbors[kColumn19Row295Neighbor];
  const FieldElementT& column19_row327 = neighbors[kColumn19Row327Neighbor];
  const FieldElementT& column19_row391 = neighbors[kColumn19Row391Neighbor];
  const FieldElementT& column19_row423 = neighbors[kColumn19Row423Neighbor];
  const FieldElementT& column19_row455 = neighbors[kColumn19Row455Neighbor];
  const FieldElementT& column19_row4118 = neighbors[kColumn19Row4118Neighbor];
  const FieldElementT& column19_row4119 = neighbors[kColumn19Row4119Neighbor];
  const FieldElementT& column19_row8214 = neighbors[kColumn19Row8214Neighbor];
  const FieldElementT& column20_row0 = neighbors[kColumn20Row0Neighbor];
  const FieldElementT& column20_row1 = neighbors[kColumn20Row1Neighbor];
  const FieldElementT& column20_row2 = neighbors[kColumn20Row2Neighbor];
  const FieldElementT& column20_row3 = neighbors[kColumn20Row3Neighbor];
  const FieldElementT& column21_row0 = neighbors[kColumn21Row0Neighbor];
  const FieldElementT& column21_row1 = neighbors[kColumn21Row1Neighbor];
  const FieldElementT& column21_row2 = neighbors[kColumn21Row2Neighbor];
  const FieldElementT& column21_row3 = neighbors[kColumn21Row3Neighbor];
  const FieldElementT& column21_row4 = neighbors[kColumn21Row4Neighbor];
  const FieldElementT& column21_row5 = neighbors[kColumn21Row5Neighbor];
  const FieldElementT& column21_row6 = neighbors[kColumn21Row6Neighbor];
  const FieldElementT& column21_row7 = neighbors[kColumn21Row7Neighbor];
  const FieldElementT& column21_row8 = neighbors[kColumn21Row8Neighbor];
  const FieldElementT& column21_row9 = neighbors[kColumn21Row9Neighbor];
  const FieldElementT& column21_row10 = neighbors[kColumn21Row10Neighbor];
  const FieldElementT& column21_row11 = neighbors[kColumn21Row11Neighbor];
  const FieldElementT& column21_row12 = neighbors[kColumn21Row12Neighbor];
  const FieldElementT& column21_row13 = neighbors[kColumn21Row13Neighbor];
  const FieldElementT& column21_row14 = neighbors[kColumn21Row14Neighbor];
  const FieldElementT& column21_row15 = neighbors[kColumn21Row15Neighbor];
  const FieldElementT& column21_row16 = neighbors[kColumn21Row16Neighbor];
  const FieldElementT& column21_row17 = neighbors[kColumn21Row17Neighbor];
  const FieldElementT& column21_row21 = neighbors[kColumn21Row21Neighbor];
  const FieldElementT& column21_row22 = neighbors[kColumn21Row22Neighbor];
  const FieldElementT& column21_row23 = neighbors[kColumn21Row23Neighbor];
  const FieldElementT& column21_row24 = neighbors[kColumn21Row24Neighbor];
  const FieldElementT& column21_row25 = neighbors[kColumn21Row25Neighbor];
  const FieldElementT& column21_row30 = neighbors[kColumn21Row30Neighbor];
  const FieldElementT& column21_row31 = neighbors[kColumn21Row31Neighbor];
  const FieldElementT& column21_row39 = neighbors[kColumn21Row39Neighbor];
  const FieldElementT& column21_row47 = neighbors[kColumn21Row47Neighbor];
  const FieldElementT& column21_row55 = neighbors[kColumn21Row55Neighbor];
  const FieldElementT& column21_row4081 = neighbors[kColumn21Row4081Neighbor];
  const FieldElementT& column21_row4083 = neighbors[kColumn21Row4083Neighbor];
  const FieldElementT& column21_row4089 = neighbors[kColumn21Row4089Neighbor];
  const FieldElementT& column21_row4091 = neighbors[kColumn21Row4091Neighbor];
  const FieldElementT& column21_row4093 = neighbors[kColumn21Row4093Neighbor];
  const FieldElementT& column21_row4102 = neighbors[kColumn21Row4102Neighbor];
  const FieldElementT& column21_row4110 = neighbors[kColumn21Row4110Neighbor];
  const FieldElementT& column21_row8167 = neighbors[kColumn21Row8167Neighbor];
  const FieldElementT& column21_row8177 = neighbors[kColumn21Row8177Neighbor];
  const FieldElementT& column21_row8179 = neighbors[kColumn21Row8179Neighbor];
  const FieldElementT& column21_row8183 = neighbors[kColumn21Row8183Neighbor];
  const FieldElementT& column21_row8185 = neighbors[kColumn21Row8185Neighbor];
  const FieldElementT& column21_row8187 = neighbors[kColumn21Row8187Neighbor];
  const FieldElementT& column21_row8191 = neighbors[kColumn21Row8191Neighbor];
  const FieldElementT& column22_row0 = neighbors[kColumn22Row0Neighbor];
  const FieldElementT& column22_row16 = neighbors[kColumn22Row16Neighbor];
  const FieldElementT& column22_row80 = neighbors[kColumn22Row80Neighbor];
  const FieldElementT& column22_row144 = neighbors[kColumn22Row144Neighbor];
  const FieldElementT& column22_row208 = neighbors[kColumn22Row208Neighbor];
  const FieldElementT& column22_row8160 = neighbors[kColumn22Row8160Neighbor];
  const FieldElementT& column23_inter1_row0 = neighbors[kColumn23Inter1Row0Neighbor];
  const FieldElementT& column23_inter1_row1 = neighbors[kColumn23Inter1Row1Neighbor];
  const FieldElementT& column24_inter1_row0 = neighbors[kColumn24Inter1Row0Neighbor];
  const FieldElementT& column24_inter1_row2 = neighbors[kColumn24Inter1Row2Neighbor];

  ASSERT_VERIFIER(periodic_columns.size() == 4, "periodic_columns should contain 4 elements.");
  const FieldElementT& pedersen__points__x = periodic_columns[kPedersenPointsXPeriodicColumn];
  const FieldElementT& pedersen__points__y = periodic_columns[kPedersenPointsYPeriodicColumn];
  const FieldElementT& ecdsa__generator_points__x =
      periodic_columns[kEcdsaGeneratorPointsXPeriodicColumn];
  const FieldElementT& ecdsa__generator_points__y =
      periodic_columns[kEcdsaGeneratorPointsYPeriodicColumn];

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
      ((column19_row0) + (cpu__decode__opcode_rc__bit_2)) + (FieldElementT::One());
  const FieldElementT cpu__decode__opcode_rc__bit_10 =
      (column1_row10) - ((column1_row11) + (column1_row11));
  const FieldElementT cpu__decode__opcode_rc__bit_11 =
      (column1_row11) - ((column1_row12) + (column1_row12));
  const FieldElementT cpu__decode__opcode_rc__bit_14 =
      (column1_row14) - ((column1_row15) + (column1_row15));
  const FieldElementT memory__address_diff_0 = (column20_row2) - (column20_row0);
  const FieldElementT rc16__diff_0 = (column2_row1) - (column2_row0);
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
  const FieldElementT rc_builtin__value0_0 = column0_row12;
  const FieldElementT rc_builtin__value1_0 =
      ((rc_builtin__value0_0) * (offset_size_)) + (column0_row28);
  const FieldElementT rc_builtin__value2_0 =
      ((rc_builtin__value1_0) * (offset_size_)) + (column0_row44);
  const FieldElementT rc_builtin__value3_0 =
      ((rc_builtin__value2_0) * (offset_size_)) + (column0_row60);
  const FieldElementT rc_builtin__value4_0 =
      ((rc_builtin__value3_0) * (offset_size_)) + (column0_row76);
  const FieldElementT rc_builtin__value5_0 =
      ((rc_builtin__value4_0) * (offset_size_)) + (column0_row92);
  const FieldElementT rc_builtin__value6_0 =
      ((rc_builtin__value5_0) * (offset_size_)) + (column0_row108);
  const FieldElementT rc_builtin__value7_0 =
      ((rc_builtin__value6_0) * (offset_size_)) + (column0_row124);
  const FieldElementT ecdsa__signature0__doubling_key__x_squared =
      (column21_row6) * (column21_row6);
  const FieldElementT ecdsa__signature0__exponentiate_generator__bit_0 =
      (column21_row15) - ((column21_row47) + (column21_row47));
  const FieldElementT ecdsa__signature0__exponentiate_generator__bit_neg_0 =
      (FieldElementT::One()) - (ecdsa__signature0__exponentiate_generator__bit_0);
  const FieldElementT ecdsa__signature0__exponentiate_key__bit_0 =
      (column21_row5) - ((column21_row21) + (column21_row21));
  const FieldElementT ecdsa__signature0__exponentiate_key__bit_neg_0 =
      (FieldElementT::One()) - (ecdsa__signature0__exponentiate_key__bit_0);
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
      // Compute a sum of constraints with numerator = domain18.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc16/perm/step0:
        const FieldElementT constraint =
            (((rc16__perm__interaction_elm_) - (column2_row1)) * (column23_inter1_row1)) -
            (((rc16__perm__interaction_elm_) - (column0_row1)) * (column23_inter1_row0));
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
      // Compute a sum of constraints with numerator = domain7.
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
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column4_row0) - (pedersen__points__y))) -
            ((column15_row0) * ((column3_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[56] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column15_row0) * (column15_row0)) -
            ((pedersen__hash0__ec_subset_sum__bit_0) *
             (((column3_row0) + (pedersen__points__x)) + (column3_row1)));
        inner_sum += random_coefficients[57] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column4_row0) + (column4_row1))) -
            ((column15_row0) * ((column3_row0) - (column3_row1)));
        inner_sum += random_coefficients[58] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column3_row1) - (column3_row0));
        inner_sum += random_coefficients[59] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column4_row1) - (column4_row0));
        inner_sum += random_coefficients[60] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/booleanity_test:
        const FieldElementT constraint =
            (pedersen__hash1__ec_subset_sum__bit_0) *
            ((pedersen__hash1__ec_subset_sum__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[71] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/add_points/slope:
        const FieldElementT constraint =
            ((pedersen__hash1__ec_subset_sum__bit_0) * ((column7_row0) - (pedersen__points__y))) -
            ((column16_row0) * ((column6_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[74] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column16_row0) * (column16_row0)) -
            ((pedersen__hash1__ec_subset_sum__bit_0) *
             (((column6_row0) + (pedersen__points__x)) + (column6_row1)));
        inner_sum += random_coefficients[75] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash1__ec_subset_sum__bit_0) * ((column7_row0) + (column7_row1))) -
            ((column16_row0) * ((column6_row0) - (column6_row1)));
        inner_sum += random_coefficients[76] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash1__ec_subset_sum__bit_neg_0) * ((column6_row1) - (column6_row0));
        inner_sum += random_coefficients[77] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash1__ec_subset_sum__bit_neg_0) * ((column7_row1) - (column7_row0));
        inner_sum += random_coefficients[78] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/booleanity_test:
        const FieldElementT constraint =
            (pedersen__hash2__ec_subset_sum__bit_0) *
            ((pedersen__hash2__ec_subset_sum__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[89] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/add_points/slope:
        const FieldElementT constraint =
            ((pedersen__hash2__ec_subset_sum__bit_0) * ((column10_row0) - (pedersen__points__y))) -
            ((column17_row0) * ((column9_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[92] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column17_row0) * (column17_row0)) -
            ((pedersen__hash2__ec_subset_sum__bit_0) *
             (((column9_row0) + (pedersen__points__x)) + (column9_row1)));
        inner_sum += random_coefficients[93] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash2__ec_subset_sum__bit_0) * ((column10_row0) + (column10_row1))) -
            ((column17_row0) * ((column9_row0) - (column9_row1)));
        inner_sum += random_coefficients[94] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash2__ec_subset_sum__bit_neg_0) * ((column9_row1) - (column9_row0));
        inner_sum += random_coefficients[95] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash2__ec_subset_sum__bit_neg_0) * ((column10_row1) - (column10_row0));
        inner_sum += random_coefficients[96] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/booleanity_test:
        const FieldElementT constraint =
            (pedersen__hash3__ec_subset_sum__bit_0) *
            ((pedersen__hash3__ec_subset_sum__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[107] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/add_points/slope:
        const FieldElementT constraint =
            ((pedersen__hash3__ec_subset_sum__bit_0) * ((column13_row0) - (pedersen__points__y))) -
            ((column18_row0) * ((column12_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[110] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column18_row0) * (column18_row0)) -
            ((pedersen__hash3__ec_subset_sum__bit_0) *
             (((column12_row0) + (pedersen__points__x)) + (column12_row1)));
        inner_sum += random_coefficients[111] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash3__ec_subset_sum__bit_0) * ((column13_row0) + (column13_row1))) -
            ((column18_row0) * ((column12_row0) - (column12_row1)));
        inner_sum += random_coefficients[112] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash3__ec_subset_sum__bit_neg_0) * ((column12_row1) - (column12_row0));
        inner_sum += random_coefficients[113] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash3__ec_subset_sum__bit_neg_0) * ((column13_row1) - (column13_row0));
        inner_sum += random_coefficients[114] * constraint;
      }
      outer_sum += inner_sum * domain7;
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
            (column19_row1) -
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
            ((column19_row8) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_0) * (column21_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_0)) * (column21_row0))) +
             (column0_row0));
        inner_sum += random_coefficients[7] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem0_addr:
        const FieldElementT constraint =
            ((column19_row4) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_1) * (column21_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_1)) * (column21_row0))) +
             (column0_row8));
        inner_sum += random_coefficients[8] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem1_addr:
        const FieldElementT constraint = ((column19_row12) + (half_offset_size_)) -
                                         ((((((cpu__decode__opcode_rc__bit_2) * (column19_row0)) +
                                             ((cpu__decode__opcode_rc__bit_4) * (column21_row0))) +
                                            ((cpu__decode__opcode_rc__bit_3) * (column21_row8))) +
                                           ((cpu__decode__flag_op1_base_op0_0) * (column19_row5))) +
                                          (column0_row4));
        inner_sum += random_coefficients[9] * constraint;
      }
      {
        // Constraint expression for cpu/operands/ops_mul:
        const FieldElementT constraint = (column21_row4) - ((column19_row5) * (column19_row13));
        inner_sum += random_coefficients[10] * constraint;
      }
      {
        // Constraint expression for cpu/operands/res:
        const FieldElementT constraint =
            (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column21_row12)) -
            ((((cpu__decode__opcode_rc__bit_5) * ((column19_row5) + (column19_row13))) +
              ((cpu__decode__opcode_rc__bit_6) * (column21_row4))) +
             ((cpu__decode__flag_res_op1_0) * (column19_row13)));
        inner_sum += random_coefficients[11] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_fp:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column19_row9) - (column21_row8));
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
            (cpu__decode__opcode_rc__bit_14) * ((column19_row9) - (column21_row12));
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
            (column21_row2) - ((cpu__decode__opcode_rc__bit_9) * (column19_row9));
        inner_sum += random_coefficients[12] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp1:
        const FieldElementT constraint = (column21_row10) - ((column21_row2) * (column21_row12));
        inner_sum += random_coefficients[13] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_negative:
        const FieldElementT constraint =
            ((((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column19_row16)) +
             ((column21_row2) * ((column19_row16) - ((column19_row0) + (column19_row13))))) -
            ((((cpu__decode__flag_pc_update_regular_0) * (npc_reg_0)) +
              ((cpu__decode__opcode_rc__bit_7) * (column21_row12))) +
             ((cpu__decode__opcode_rc__bit_8) * ((column19_row0) + (column21_row12))));
        inner_sum += random_coefficients[14] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_positive:
        const FieldElementT constraint =
            ((column21_row10) - (cpu__decode__opcode_rc__bit_9)) * ((column19_row16) - (npc_reg_0));
        inner_sum += random_coefficients[15] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_ap/ap_update:
        const FieldElementT constraint =
            (column21_row16) -
            ((((column21_row0) + ((cpu__decode__opcode_rc__bit_10) * (column21_row12))) +
              (cpu__decode__opcode_rc__bit_11)) +
             ((cpu__decode__opcode_rc__bit_12) * (FieldElementT::ConstexprFromBigInt(0x2_Z))));
        inner_sum += random_coefficients[16] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_fp/fp_update:
        const FieldElementT constraint =
            (column21_row24) - ((((cpu__decode__fp_update_regular_0) * (column21_row8)) +
                                 ((cpu__decode__opcode_rc__bit_13) * (column19_row9))) +
                                ((cpu__decode__opcode_rc__bit_12) *
                                 ((column21_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z)))));
        inner_sum += random_coefficients[17] * constraint;
      }
      outer_sum += inner_sum * domain19;
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
                                         (((column21_row14) + (column21_row14)) * (column21_row13));
        inner_sum += random_coefficients[138] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/doubling_key/x:
        const FieldElementT constraint = ((column21_row13) * (column21_row13)) -
                                         (((column21_row6) + (column21_row6)) + (column21_row22));
        inner_sum += random_coefficients[139] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/doubling_key/y:
        const FieldElementT constraint = ((column21_row14) + (column21_row30)) -
                                         ((column21_row13) * ((column21_row6) - (column21_row22)));
        inner_sum += random_coefficients[140] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/booleanity_test:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_0) *
            ((ecdsa__signature0__exponentiate_key__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[150] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/slope:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_key__bit_0) * ((column21_row9) - (column21_row14))) -
            ((column21_row3) * ((column21_row1) - (column21_row6)));
        inner_sum += random_coefficients[153] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/x:
        const FieldElementT constraint = ((column21_row3) * (column21_row3)) -
                                         ((ecdsa__signature0__exponentiate_key__bit_0) *
                                          (((column21_row1) + (column21_row6)) + (column21_row17)));
        inner_sum += random_coefficients[154] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/y:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_key__bit_0) * ((column21_row9) + (column21_row25))) -
            ((column21_row3) * ((column21_row1) - (column21_row17)));
        inner_sum += random_coefficients[155] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column21_row11) * ((column21_row1) - (column21_row6))) - (FieldElementT::One());
        inner_sum += random_coefficients[156] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/copy_point/x:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_neg_0) * ((column21_row17) - (column21_row1));
        inner_sum += random_coefficients[157] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/copy_point/y:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_neg_0) * ((column21_row25) - (column21_row9));
        inner_sum += random_coefficients[158] * constraint;
      }
      outer_sum += inner_sum * domain12;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain4);
  }

  {
    // Compute a sum of constraints with denominator = domain20.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for initial_ap:
        const FieldElementT constraint = (column21_row0) - (initial_ap_);
        inner_sum += random_coefficients[27] * constraint;
      }
      {
        // Constraint expression for initial_fp:
        const FieldElementT constraint = (column21_row8) - (initial_ap_);
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
                ((column20_row0) +
                 ((memory__multi_column_perm__hash_interaction_elm0_) * (column20_row1)))) *
               (column24_inter1_row0)) +
              (column19_row0)) +
             ((memory__multi_column_perm__hash_interaction_elm0_) * (column19_row1))) -
            (memory__multi_column_perm__perm__interaction_elm_);
        inner_sum += random_coefficients[33] * constraint;
      }
      {
        // Constraint expression for memory/initial_addr:
        const FieldElementT constraint = (column20_row0) - (FieldElementT::One());
        inner_sum += random_coefficients[38] * constraint;
      }
      {
        // Constraint expression for rc16/perm/init0:
        const FieldElementT constraint =
            ((((rc16__perm__interaction_elm_) - (column2_row0)) * (column23_inter1_row0)) +
             (column0_row0)) -
            (rc16__perm__interaction_elm_);
        inner_sum += random_coefficients[41] * constraint;
      }
      {
        // Constraint expression for rc16/minimum:
        const FieldElementT constraint = (column2_row0) - (rc_min_);
        inner_sum += random_coefficients[45] * constraint;
      }
      {
        // Constraint expression for pedersen/init_addr:
        const FieldElementT constraint = (column19_row6) - (initial_pedersen_addr_);
        inner_sum += random_coefficients[124] * constraint;
      }
      {
        // Constraint expression for rc_builtin/init_addr:
        const FieldElementT constraint = (column19_row102) - (initial_rc_addr_);
        inner_sum += random_coefficients[137] * constraint;
      }
      {
        // Constraint expression for ecdsa/init_addr:
        const FieldElementT constraint = (column19_row22) - (initial_ecdsa_addr_);
        inner_sum += random_coefficients[174] * constraint;
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
        const FieldElementT constraint = (column21_row0) - (final_ap_);
        inner_sum += random_coefficients[30] * constraint;
      }
      {
        // Constraint expression for final_fp:
        const FieldElementT constraint = (column21_row8) - (initial_ap_);
        inner_sum += random_coefficients[31] * constraint;
      }
      {
        // Constraint expression for final_pc:
        const FieldElementT constraint = (column19_row0) - (final_pc_);
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
              ((column20_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column20_row3)))) *
             (column24_inter1_row2)) -
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column19_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column19_row3)))) *
             (column24_inter1_row0));
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
                                         ((column20_row1) - (column20_row3));
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
            (column24_inter1_row0) - (memory__multi_column_perm__perm__public_memory_prod_);
        inner_sum += random_coefficients[35] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain21);
  }

  {
    // Compute a sum of constraints with denominator = domain2.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
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
        const FieldElementT constraint = (column23_inter1_row0) - (rc16__perm__public_memory_prod_);
        inner_sum += random_coefficients[43] * constraint;
      }
      {
        // Constraint expression for rc16/maximum:
        const FieldElementT constraint = (column2_row0) - (rc_max_);
        inner_sum += random_coefficients[46] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain18);
  }

  {
    // Compute a sum of constraints with denominator = domain8.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column16_row255) * ((column5_row0) - ((column5_row1) + (column5_row1)));
        inner_sum += random_coefficients[47] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column16_row255) *
            ((column5_row1) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column5_row192)));
        inner_sum += random_coefficients[48] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column16_row255) -
            ((column15_row255) * ((column5_row192) - ((column5_row193) + (column5_row193))));
        inner_sum += random_coefficients[49] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column15_row255) *
            ((column5_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column5_row196)));
        inner_sum += random_coefficients[50] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column15_row255) - (((column5_row251) - ((column5_row252) + (column5_row252))) *
                                 ((column5_row196) - ((column5_row197) + (column5_row197))));
        inner_sum += random_coefficients[51] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column5_row251) - ((column5_row252) + (column5_row252))) *
            ((column5_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column5_row251)));
        inner_sum += random_coefficients[52] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column18_row255) * ((column8_row0) - ((column8_row1) + (column8_row1)));
        inner_sum += random_coefficients[65] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash1/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column18_row255) *
            ((column8_row1) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column8_row192)));
        inner_sum += random_coefficients[66] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column18_row255) -
            ((column17_row255) * ((column8_row192) - ((column8_row193) + (column8_row193))));
        inner_sum += random_coefficients[67] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash1/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column17_row255) *
            ((column8_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column8_row196)));
        inner_sum += random_coefficients[68] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column17_row255) - (((column8_row251) - ((column8_row252) + (column8_row252))) *
                                 ((column8_row196) - ((column8_row197) + (column8_row197))));
        inner_sum += random_coefficients[69] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash1/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column8_row251) - ((column8_row252) + (column8_row252))) *
            ((column8_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column8_row251)));
        inner_sum += random_coefficients[70] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column22_row144) * ((column11_row0) - ((column11_row1) + (column11_row1)));
        inner_sum += random_coefficients[83] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash2/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column22_row144) *
            ((column11_row1) - ((FieldElementT::ConstexprFromBigInt(
                                    0x800000000000000000000000000000000000000000000000_Z)) *
                                (column11_row192)));
        inner_sum += random_coefficients[84] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column22_row144) -
            ((column22_row16) * ((column11_row192) - ((column11_row193) + (column11_row193))));
        inner_sum += random_coefficients[85] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash2/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column22_row16) *
            ((column11_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column11_row196)));
        inner_sum += random_coefficients[86] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column22_row16) - (((column11_row251) - ((column11_row252) + (column11_row252))) *
                                ((column11_row196) - ((column11_row197) + (column11_row197))));
        inner_sum += random_coefficients[87] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash2/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column11_row251) - ((column11_row252) + (column11_row252))) *
            ((column11_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column11_row251)));
        inner_sum += random_coefficients[88] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column22_row208) * ((column14_row0) - ((column14_row1) + (column14_row1)));
        inner_sum += random_coefficients[101] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash3/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column22_row208) *
            ((column14_row1) - ((FieldElementT::ConstexprFromBigInt(
                                    0x800000000000000000000000000000000000000000000000_Z)) *
                                (column14_row192)));
        inner_sum += random_coefficients[102] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column22_row208) -
            ((column22_row80) * ((column14_row192) - ((column14_row193) + (column14_row193))));
        inner_sum += random_coefficients[103] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash3/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column22_row80) *
            ((column14_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column14_row196)));
        inner_sum += random_coefficients[104] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column22_row80) - (((column14_row251) - ((column14_row252) + (column14_row252))) *
                                ((column14_row196) - ((column14_row197) + (column14_row197))));
        inner_sum += random_coefficients[105] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash3/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column14_row251) - ((column14_row252) + (column14_row252))) *
            ((column14_row197) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column14_row251)));
        inner_sum += random_coefficients[106] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain10.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/copy_point/x:
        const FieldElementT constraint = (column3_row256) - (column3_row255);
        inner_sum += random_coefficients[61] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/copy_point/y:
        const FieldElementT constraint = (column4_row256) - (column4_row255);
        inner_sum += random_coefficients[62] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/copy_point/x:
        const FieldElementT constraint = (column6_row256) - (column6_row255);
        inner_sum += random_coefficients[79] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/copy_point/y:
        const FieldElementT constraint = (column7_row256) - (column7_row255);
        inner_sum += random_coefficients[80] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/copy_point/x:
        const FieldElementT constraint = (column9_row256) - (column9_row255);
        inner_sum += random_coefficients[97] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/copy_point/y:
        const FieldElementT constraint = (column10_row256) - (column10_row255);
        inner_sum += random_coefficients[98] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/copy_point/x:
        const FieldElementT constraint = (column12_row256) - (column12_row255);
        inner_sum += random_coefficients[115] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/copy_point/y:
        const FieldElementT constraint = (column13_row256) - (column13_row255);
        inner_sum += random_coefficients[116] * constraint;
      }
      outer_sum += inner_sum * domain10;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain8);
  }

  {
    // Compute a sum of constraints with denominator = domain9.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column5_row0;
        inner_sum += random_coefficients[54] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column8_row0;
        inner_sum += random_coefficients[72] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column11_row0;
        inner_sum += random_coefficients[90] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column14_row0;
        inner_sum += random_coefficients[108] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain9);
  }

  {
    // Compute a sum of constraints with denominator = domain7.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column5_row0;
        inner_sum += random_coefficients[55] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column8_row0;
        inner_sum += random_coefficients[73] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column11_row0;
        inner_sum += random_coefficients[91] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column14_row0;
        inner_sum += random_coefficients[109] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain7);
  }

  {
    // Compute a sum of constraints with denominator = domain11.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/init/x:
        const FieldElementT constraint = (column3_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[63] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/init/y:
        const FieldElementT constraint = (column4_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[64] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/init/x:
        const FieldElementT constraint = (column6_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[81] * constraint;
      }
      {
        // Constraint expression for pedersen/hash1/init/y:
        const FieldElementT constraint = (column7_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[82] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/init/x:
        const FieldElementT constraint = (column9_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[99] * constraint;
      }
      {
        // Constraint expression for pedersen/hash2/init/y:
        const FieldElementT constraint = (column10_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[100] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/init/x:
        const FieldElementT constraint = (column12_row0) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[117] * constraint;
      }
      {
        // Constraint expression for pedersen/hash3/init/y:
        const FieldElementT constraint = (column13_row0) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[118] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value0:
        const FieldElementT constraint = (column19_row7) - (column5_row0);
        inner_sum += random_coefficients[119] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value1:
        const FieldElementT constraint = (column19_row135) - (column8_row0);
        inner_sum += random_coefficients[120] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value2:
        const FieldElementT constraint = (column19_row263) - (column11_row0);
        inner_sum += random_coefficients[121] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value3:
        const FieldElementT constraint = (column19_row391) - (column14_row0);
        inner_sum += random_coefficients[122] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value0:
        const FieldElementT constraint = (column19_row71) - (column5_row256);
        inner_sum += random_coefficients[125] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value1:
        const FieldElementT constraint = (column19_row199) - (column8_row256);
        inner_sum += random_coefficients[126] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value2:
        const FieldElementT constraint = (column19_row327) - (column11_row256);
        inner_sum += random_coefficients[127] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value3:
        const FieldElementT constraint = (column19_row455) - (column14_row256);
        inner_sum += random_coefficients[128] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value0:
        const FieldElementT constraint = (column19_row39) - (column3_row511);
        inner_sum += random_coefficients[130] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value1:
        const FieldElementT constraint = (column19_row167) - (column6_row511);
        inner_sum += random_coefficients[131] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value2:
        const FieldElementT constraint = (column19_row295) - (column9_row511);
        inner_sum += random_coefficients[132] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value3:
        const FieldElementT constraint = (column19_row423) - (column12_row511);
        inner_sum += random_coefficients[133] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain11);
  }

  {
    // Compute a sum of constraints with denominator = domain6.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain22.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input0_addr:
        const FieldElementT constraint =
            (column19_row134) - ((column19_row38) + (FieldElementT::One()));
        inner_sum += random_coefficients[123] * constraint;
      }
      {
        // Constraint expression for rc_builtin/addr_step:
        const FieldElementT constraint =
            (column19_row230) - ((column19_row102) + (FieldElementT::One()));
        inner_sum += random_coefficients[136] * constraint;
      }
      outer_sum += inner_sum * domain22;
    }

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input1_addr:
        const FieldElementT constraint =
            (column19_row70) - ((column19_row6) + (FieldElementT::One()));
        inner_sum += random_coefficients[129] * constraint;
      }
      {
        // Constraint expression for pedersen/output_addr:
        const FieldElementT constraint =
            (column19_row38) - ((column19_row70) + (FieldElementT::One()));
        inner_sum += random_coefficients[134] * constraint;
      }
      {
        // Constraint expression for rc_builtin/value:
        const FieldElementT constraint = (rc_builtin__value7_0) - (column19_row103);
        inner_sum += random_coefficients[135] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain6);
  }

  {
    // Compute a sum of constraints with denominator = domain5.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain15.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/booleanity_test:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_generator__bit_0) *
            ((ecdsa__signature0__exponentiate_generator__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[141] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/slope:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_generator__bit_0) *
             ((column21_row23) - (ecdsa__generator_points__y))) -
            ((column21_row31) * ((column21_row7) - (ecdsa__generator_points__x)));
        inner_sum += random_coefficients[144] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/x:
        const FieldElementT constraint =
            ((column21_row31) * (column21_row31)) -
            ((ecdsa__signature0__exponentiate_generator__bit_0) *
             (((column21_row7) + (ecdsa__generator_points__x)) + (column21_row39)));
        inner_sum += random_coefficients[145] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/y:
        const FieldElementT constraint = ((ecdsa__signature0__exponentiate_generator__bit_0) *
                                          ((column21_row23) + (column21_row55))) -
                                         ((column21_row31) * ((column21_row7) - (column21_row39)));
        inner_sum += random_coefficients[146] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column22_row0) * ((column21_row7) - (ecdsa__generator_points__x))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[147] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/copy_point/x:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_generator__bit_neg_0) *
                                         ((column21_row39) - (column21_row7));
        inner_sum += random_coefficients[148] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/copy_point/y:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_generator__bit_neg_0) *
                                         ((column21_row55) - (column21_row23));
        inner_sum += random_coefficients[149] * constraint;
      }
      outer_sum += inner_sum * domain15;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain5);
  }

  {
    // Compute a sum of constraints with denominator = domain16.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/bit_extraction_end:
        const FieldElementT constraint = column21_row15;
        inner_sum += random_coefficients[142] * constraint;
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
        // Constraint expression for ecdsa/signature0/exponentiate_generator/zeros_tail:
        const FieldElementT constraint = column21_row15;
        inner_sum += random_coefficients[143] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain15);
  }

  {
    // Compute a sum of constraints with denominator = domain13.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/bit_extraction_end:
        const FieldElementT constraint = column21_row5;
        inner_sum += random_coefficients[151] * constraint;
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
        const FieldElementT constraint = column21_row5;
        inner_sum += random_coefficients[152] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain12);
  }

  {
    // Compute a sum of constraints with denominator = domain17.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/init_gen/x:
        const FieldElementT constraint = (column21_row7) - (((ecdsa__sig_config_).shift_point).x);
        inner_sum += random_coefficients[159] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/init_gen/y:
        const FieldElementT constraint = (column21_row23) + (((ecdsa__sig_config_).shift_point).y);
        inner_sum += random_coefficients[160] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/slope:
        const FieldElementT constraint =
            (column21_row8183) -
            ((column21_row4089) + ((column21_row8191) * ((column21_row8167) - (column21_row4081))));
        inner_sum += random_coefficients[163] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/x:
        const FieldElementT constraint =
            ((column21_row8191) * (column21_row8191)) -
            (((column21_row8167) + (column21_row4081)) + (column21_row4102));
        inner_sum += random_coefficients[164] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/y:
        const FieldElementT constraint =
            ((column21_row8183) + (column21_row4110)) -
            ((column21_row8191) * ((column21_row8167) - (column21_row4102)));
        inner_sum += random_coefficients[165] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/x_diff_inv:
        const FieldElementT constraint =
            ((column22_row8160) * ((column21_row8167) - (column21_row4081))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[166] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/slope:
        const FieldElementT constraint =
            ((column21_row8185) + (((ecdsa__sig_config_).shift_point).y)) -
            ((column21_row4083) * ((column21_row8177) - (((ecdsa__sig_config_).shift_point).x)));
        inner_sum += random_coefficients[167] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/x:
        const FieldElementT constraint =
            ((column21_row4083) * (column21_row4083)) -
            (((column21_row8177) + (((ecdsa__sig_config_).shift_point).x)) + (column21_row5));
        inner_sum += random_coefficients[168] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/x_diff_inv:
        const FieldElementT constraint =
            ((column21_row8179) * ((column21_row8177) - (((ecdsa__sig_config_).shift_point).x))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[169] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/z_nonzero:
        const FieldElementT constraint =
            ((column21_row15) * (column21_row4091)) - (FieldElementT::One());
        inner_sum += random_coefficients[170] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/q_on_curve/x_squared:
        const FieldElementT constraint = (column21_row8187) - ((column21_row6) * (column21_row6));
        inner_sum += random_coefficients[172] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/q_on_curve/on_curve:
        const FieldElementT constraint = ((column21_row14) * (column21_row14)) -
                                         ((((column21_row6) * (column21_row8187)) +
                                           (((ecdsa__sig_config_).alpha) * (column21_row6))) +
                                          ((ecdsa__sig_config_).beta));
        inner_sum += random_coefficients[173] * constraint;
      }
      {
        // Constraint expression for ecdsa/message_addr:
        const FieldElementT constraint =
            (column19_row4118) - ((column19_row22) + (FieldElementT::One()));
        inner_sum += random_coefficients[175] * constraint;
      }
      {
        // Constraint expression for ecdsa/message_value0:
        const FieldElementT constraint = (column19_row4119) - (column21_row15);
        inner_sum += random_coefficients[177] * constraint;
      }
      {
        // Constraint expression for ecdsa/pubkey_value0:
        const FieldElementT constraint = (column19_row23) - (column21_row6);
        inner_sum += random_coefficients[178] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain23.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/pubkey_addr:
        const FieldElementT constraint =
            (column19_row8214) - ((column19_row4118) + (FieldElementT::One()));
        inner_sum += random_coefficients[176] * constraint;
      }
      outer_sum += inner_sum * domain23;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain17);
  }

  {
    // Compute a sum of constraints with denominator = domain14.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/init_key/x:
        const FieldElementT constraint = (column21_row1) - (((ecdsa__sig_config_).shift_point).x);
        inner_sum += random_coefficients[161] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/init_key/y:
        const FieldElementT constraint = (column21_row9) - (((ecdsa__sig_config_).shift_point).y);
        inner_sum += random_coefficients[162] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/r_and_w_nonzero:
        const FieldElementT constraint =
            ((column21_row5) * (column21_row4093)) - (FieldElementT::One());
        inner_sum += random_coefficients[171] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain14);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> CpuAirDefinition<FieldElementT, 0>::DomainEvalsAtPoint(
    gsl::span<const FieldElementT> point_powers,
    gsl::span<const FieldElementT> shifts) const {
  const FieldElementT& domain0 = (point_powers[1]) - (FieldElementT::One());
  const FieldElementT& domain1 = (point_powers[2]) - (FieldElementT::One());
  const FieldElementT& domain2 = (point_powers[3]) - (FieldElementT::One());
  const FieldElementT& domain3 = (point_powers[4]) - (shifts[0]);
  const FieldElementT& domain4 = (point_powers[4]) - (FieldElementT::One());
  const FieldElementT& domain5 = (point_powers[5]) - (FieldElementT::One());
  const FieldElementT& domain6 = (point_powers[6]) - (FieldElementT::One());
  const FieldElementT& domain7 = (point_powers[7]) - (shifts[1]);
  const FieldElementT& domain8 = (point_powers[7]) - (FieldElementT::One());
  const FieldElementT& domain9 = (point_powers[7]) - (shifts[2]);
  const FieldElementT& domain10 = (point_powers[8]) - (shifts[3]);
  const FieldElementT& domain11 = (point_powers[8]) - (FieldElementT::One());
  const FieldElementT& domain12 = (point_powers[9]) - (shifts[1]);
  const FieldElementT& domain13 = (point_powers[9]) - (shifts[4]);
  const FieldElementT& domain14 = (point_powers[9]) - (FieldElementT::One());
  const FieldElementT& domain15 = (point_powers[10]) - (shifts[1]);
  const FieldElementT& domain16 = (point_powers[10]) - (shifts[4]);
  const FieldElementT& domain17 = (point_powers[10]) - (FieldElementT::One());
  return {
      domain0, domain1,  domain2,  domain3,  domain4,  domain5,  domain6,  domain7,  domain8,
      domain9, domain10, domain11, domain12, domain13, domain14, domain15, domain16, domain17,
  };
}

template <typename FieldElementT>
TraceGenerationContext CpuAirDefinition<FieldElementT, 0>::GetTraceGenerationContext() const {
  TraceGenerationContext ctx;

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
      "memory/sorted/addr",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/sorted/value",
      VirtualColumn(/*column=*/kColumn20Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/registers/ap", VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/registers/fp", VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/operands/ops_mul",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/res",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp0",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp1",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/10));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/key_points/x",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/key_points/y",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/14));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/partial_sum/x",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/partial_sum/y",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/selector",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/doubling_slope",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/slope",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/x_diff_inv",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/16, /*row_offset=*/11));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/partial_sum/x",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/32, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/partial_sum/y",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/32, /*row_offset=*/23));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/selector",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/32, /*row_offset=*/15));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/slope",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/32, /*row_offset=*/31));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/x_diff_inv",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/32, /*row_offset=*/0));
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
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/256, /*row_offset=*/16));
  ctx.AddVirtualColumn(
      "pedersen/hash2/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/256, /*row_offset=*/144));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/256, /*row_offset=*/80));
  ctx.AddVirtualColumn(
      "pedersen/hash3/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/256, /*row_offset=*/208));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/r_w_inv",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/4096, /*row_offset=*/4093));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/add_results_slope",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/8192, /*row_offset=*/8191));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/add_results_inv",
      VirtualColumn(/*column=*/kColumn22Column, /*step=*/8192, /*row_offset=*/8160));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/extract_r_slope",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/8192, /*row_offset=*/4083));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/extract_r_inv",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/8192, /*row_offset=*/8179));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/z_inv",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/8192, /*row_offset=*/4091));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/q_x_squared",
      VirtualColumn(/*column=*/kColumn21Column, /*step=*/8192, /*row_offset=*/8187));
  ctx.AddVirtualColumn(
      "rc16/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn23Inter1Column - kNumColumnsFirst, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/multi_column_perm/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn24Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/0));
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
      "cpu/decode/off0", VirtualColumn(/*column=*/kColumn0Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/off1", VirtualColumn(/*column=*/kColumn0Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/decode/off2", VirtualColumn(/*column=*/kColumn0Column, /*step=*/16, /*row_offset=*/4));
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
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "orig/public_memory/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "pedersen/input0/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "pedersen/input0/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "pedersen/input1/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/70));
  ctx.AddVirtualColumn(
      "pedersen/input1/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/71));
  ctx.AddVirtualColumn(
      "pedersen/output/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/38));
  ctx.AddVirtualColumn(
      "pedersen/output/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/39));
  ctx.AddVirtualColumn(
      "rc_builtin/mem/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/102));
  ctx.AddVirtualColumn(
      "rc_builtin/mem/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/128, /*row_offset=*/103));
  ctx.AddVirtualColumn(
      "rc_builtin/inner_rc",
      VirtualColumn(/*column=*/kColumn0Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "ecdsa/pubkey/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8192, /*row_offset=*/22));
  ctx.AddVirtualColumn(
      "ecdsa/pubkey/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8192, /*row_offset=*/23));
  ctx.AddVirtualColumn(
      "ecdsa/message/addr",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8192, /*row_offset=*/4118));
  ctx.AddVirtualColumn(
      "ecdsa/message/value",
      VirtualColumn(/*column=*/kColumn19Column, /*step=*/8192, /*row_offset=*/4119));

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
std::vector<std::pair<int64_t, uint64_t>> CpuAirDefinition<FieldElementT, 0>::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;

  mask.reserve(201);
  mask.emplace_back(0, kColumn0Column);
  mask.emplace_back(1, kColumn0Column);
  mask.emplace_back(4, kColumn0Column);
  mask.emplace_back(8, kColumn0Column);
  mask.emplace_back(12, kColumn0Column);
  mask.emplace_back(28, kColumn0Column);
  mask.emplace_back(44, kColumn0Column);
  mask.emplace_back(60, kColumn0Column);
  mask.emplace_back(76, kColumn0Column);
  mask.emplace_back(92, kColumn0Column);
  mask.emplace_back(108, kColumn0Column);
  mask.emplace_back(124, kColumn0Column);
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
  mask.emplace_back(6, kColumn19Column);
  mask.emplace_back(7, kColumn19Column);
  mask.emplace_back(8, kColumn19Column);
  mask.emplace_back(9, kColumn19Column);
  mask.emplace_back(12, kColumn19Column);
  mask.emplace_back(13, kColumn19Column);
  mask.emplace_back(16, kColumn19Column);
  mask.emplace_back(22, kColumn19Column);
  mask.emplace_back(23, kColumn19Column);
  mask.emplace_back(38, kColumn19Column);
  mask.emplace_back(39, kColumn19Column);
  mask.emplace_back(70, kColumn19Column);
  mask.emplace_back(71, kColumn19Column);
  mask.emplace_back(102, kColumn19Column);
  mask.emplace_back(103, kColumn19Column);
  mask.emplace_back(134, kColumn19Column);
  mask.emplace_back(135, kColumn19Column);
  mask.emplace_back(167, kColumn19Column);
  mask.emplace_back(199, kColumn19Column);
  mask.emplace_back(230, kColumn19Column);
  mask.emplace_back(263, kColumn19Column);
  mask.emplace_back(295, kColumn19Column);
  mask.emplace_back(327, kColumn19Column);
  mask.emplace_back(391, kColumn19Column);
  mask.emplace_back(423, kColumn19Column);
  mask.emplace_back(455, kColumn19Column);
  mask.emplace_back(4118, kColumn19Column);
  mask.emplace_back(4119, kColumn19Column);
  mask.emplace_back(8214, kColumn19Column);
  mask.emplace_back(0, kColumn20Column);
  mask.emplace_back(1, kColumn20Column);
  mask.emplace_back(2, kColumn20Column);
  mask.emplace_back(3, kColumn20Column);
  mask.emplace_back(0, kColumn21Column);
  mask.emplace_back(1, kColumn21Column);
  mask.emplace_back(2, kColumn21Column);
  mask.emplace_back(3, kColumn21Column);
  mask.emplace_back(4, kColumn21Column);
  mask.emplace_back(5, kColumn21Column);
  mask.emplace_back(6, kColumn21Column);
  mask.emplace_back(7, kColumn21Column);
  mask.emplace_back(8, kColumn21Column);
  mask.emplace_back(9, kColumn21Column);
  mask.emplace_back(10, kColumn21Column);
  mask.emplace_back(11, kColumn21Column);
  mask.emplace_back(12, kColumn21Column);
  mask.emplace_back(13, kColumn21Column);
  mask.emplace_back(14, kColumn21Column);
  mask.emplace_back(15, kColumn21Column);
  mask.emplace_back(16, kColumn21Column);
  mask.emplace_back(17, kColumn21Column);
  mask.emplace_back(21, kColumn21Column);
  mask.emplace_back(22, kColumn21Column);
  mask.emplace_back(23, kColumn21Column);
  mask.emplace_back(24, kColumn21Column);
  mask.emplace_back(25, kColumn21Column);
  mask.emplace_back(30, kColumn21Column);
  mask.emplace_back(31, kColumn21Column);
  mask.emplace_back(39, kColumn21Column);
  mask.emplace_back(47, kColumn21Column);
  mask.emplace_back(55, kColumn21Column);
  mask.emplace_back(4081, kColumn21Column);
  mask.emplace_back(4083, kColumn21Column);
  mask.emplace_back(4089, kColumn21Column);
  mask.emplace_back(4091, kColumn21Column);
  mask.emplace_back(4093, kColumn21Column);
  mask.emplace_back(4102, kColumn21Column);
  mask.emplace_back(4110, kColumn21Column);
  mask.emplace_back(8167, kColumn21Column);
  mask.emplace_back(8177, kColumn21Column);
  mask.emplace_back(8179, kColumn21Column);
  mask.emplace_back(8183, kColumn21Column);
  mask.emplace_back(8185, kColumn21Column);
  mask.emplace_back(8187, kColumn21Column);
  mask.emplace_back(8191, kColumn21Column);
  mask.emplace_back(0, kColumn22Column);
  mask.emplace_back(16, kColumn22Column);
  mask.emplace_back(80, kColumn22Column);
  mask.emplace_back(144, kColumn22Column);
  mask.emplace_back(208, kColumn22Column);
  mask.emplace_back(8160, kColumn22Column);
  mask.emplace_back(0, kColumn23Inter1Column);
  mask.emplace_back(1, kColumn23Inter1Column);
  mask.emplace_back(0, kColumn24Inter1Column);
  mask.emplace_back(2, kColumn24Inter1Column);

  return mask;
}

}  // namespace cpu

}  // namespace starkware
