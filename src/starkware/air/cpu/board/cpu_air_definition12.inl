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
CpuAirDefinition<FieldElementT, 12>::CreateCompositionPolynomial(
    const FieldElement& trace_generator, const ConstFieldElementSpan& random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const FieldElementT& gen = trace_generator.As<FieldElementT>();

  const std::vector<uint64_t> point_exponents = {
      uint64_t(trace_length_),
      uint64_t(SafeDiv(trace_length_, 2)),
      uint64_t(SafeDiv(trace_length_, 4)),
      uint64_t(SafeDiv(trace_length_, 8)),
      uint64_t(SafeDiv(trace_length_, 16)),
      uint64_t(SafeDiv(trace_length_, 32)),
      uint64_t(SafeDiv(trace_length_, 64)),
      uint64_t(SafeDiv(trace_length_, 128)),
      uint64_t(SafeDiv(trace_length_, 256)),
      uint64_t(SafeDiv(trace_length_, 512)),
      uint64_t(SafeDiv(trace_length_, 1024)),
      uint64_t(SafeDiv(trace_length_, 2048)),
      uint64_t(SafeDiv(trace_length_, 4096))};
  const std::vector<uint64_t> gen_exponents = {
      uint64_t(SafeDiv((15) * (trace_length_), 16)),
      uint64_t(SafeDiv((3) * (trace_length_), 4)),
      uint64_t(SafeDiv(trace_length_, 64)),
      uint64_t(SafeDiv(trace_length_, 32)),
      uint64_t(SafeDiv((3) * (trace_length_), 64)),
      uint64_t(SafeDiv(trace_length_, 16)),
      uint64_t(SafeDiv((5) * (trace_length_), 64)),
      uint64_t(SafeDiv((3) * (trace_length_), 32)),
      uint64_t(SafeDiv((7) * (trace_length_), 64)),
      uint64_t(SafeDiv(trace_length_, 8)),
      uint64_t(SafeDiv((9) * (trace_length_), 64)),
      uint64_t(SafeDiv((5) * (trace_length_), 32)),
      uint64_t(SafeDiv((11) * (trace_length_), 64)),
      uint64_t(SafeDiv((3) * (trace_length_), 16)),
      uint64_t(SafeDiv((13) * (trace_length_), 64)),
      uint64_t(SafeDiv((7) * (trace_length_), 32)),
      uint64_t(SafeDiv((15) * (trace_length_), 64)),
      uint64_t(SafeDiv((31) * (trace_length_), 32)),
      uint64_t(SafeDiv((61) * (trace_length_), 64)),
      uint64_t(SafeDiv((63) * (trace_length_), 64)),
      uint64_t(SafeDiv((11) * (trace_length_), 16)),
      uint64_t(SafeDiv((23) * (trace_length_), 32)),
      uint64_t(SafeDiv((25) * (trace_length_), 32)),
      uint64_t(SafeDiv((13) * (trace_length_), 16)),
      uint64_t(SafeDiv((27) * (trace_length_), 32)),
      uint64_t(SafeDiv((7) * (trace_length_), 8)),
      uint64_t(SafeDiv((29) * (trace_length_), 32)),
      uint64_t(SafeDiv((19) * (trace_length_), 32)),
      uint64_t(SafeDiv((5) * (trace_length_), 8)),
      uint64_t(SafeDiv((21) * (trace_length_), 32)),
      uint64_t(SafeDiv((255) * (trace_length_), 256)),
      uint64_t(SafeDiv(trace_length_, 2)),
      uint64_t((trace_length_) - (16)),
      uint64_t((trace_length_) - (2)),
      uint64_t((trace_length_) - (4)),
      uint64_t((trace_length_) - (4096)),
      uint64_t((trace_length_) - (256)),
      uint64_t((trace_length_) - (512))};

  BuildAutoPeriodicColumns(gen, &builder);

  BuildPeriodicColumns(gen, &builder);

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
void CpuAirDefinition<FieldElementT, 12>::BuildAutoPeriodicColumns(
    const FieldElementT& gen, Builder* builder) const {
  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonFullRoundKey0PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 128),
      kPoseidonPoseidonFullRoundKey0PeriodicColumn);

  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonFullRoundKey1PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 128),
      kPoseidonPoseidonFullRoundKey1PeriodicColumn);

  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonFullRoundKey2PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 128),
      kPoseidonPoseidonFullRoundKey2PeriodicColumn);

  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonPartialRoundKey0PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 16),
      kPoseidonPoseidonPartialRoundKey0PeriodicColumn);

  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonPartialRoundKey1PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 32),
      kPoseidonPoseidonPartialRoundKey1PeriodicColumn);
};

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
CpuAirDefinition<FieldElementT, 12>::PrecomputeDomainEvalsOnCoset(
    const FieldElementT& point, const FieldElementT& generator,
    gsl::span<const uint64_t> point_exponents,
    [[maybe_unused]] gsl::span<const FieldElementT> shifts) const {
  const std::vector<FieldElementT> strict_point_powers = BatchPow(point, point_exponents);
  const std::vector<FieldElementT> gen_powers = BatchPow(generator, point_exponents);

  // point_powers[i][j] is the evaluation of the ith power at its jth point.
  // The index j runs until the order of the domain (beyond we'd cycle back to point_powers[i][0]).
  std::vector<std::vector<FieldElementT>> point_powers(point_exponents.size());
  for (size_t i = 0; i < point_exponents.size(); ++i) {
    uint64_t size = point_exponents[i] == 0 ? 0 : SafeDiv(trace_length_, point_exponents[i]);
    auto& vec = point_powers[i];
    auto power = strict_point_powers[i];
    vec.reserve(size);
    if (size > 0) {
      vec.push_back(power);
    }
    for (size_t j = 1; j < size; ++j) {
      power *= gen_powers[i];
      vec.push_back(power);
    }
  }

  [[maybe_unused]] TaskManager& task_manager = TaskManager::GetInstance();
  constexpr size_t kPeriodUpperBound = 4194305;
  constexpr size_t kTaskSize = 1024;
  size_t period;

  std::vector<std::vector<FieldElementT>> precomp_domains = {
      FieldElementT::UninitializedVector(1),    FieldElementT::UninitializedVector(2),
      FieldElementT::UninitializedVector(4),    FieldElementT::UninitializedVector(8),
      FieldElementT::UninitializedVector(16),   FieldElementT::UninitializedVector(16),
      FieldElementT::UninitializedVector(32),   FieldElementT::UninitializedVector(64),
      FieldElementT::UninitializedVector(128),  FieldElementT::UninitializedVector(256),
      FieldElementT::UninitializedVector(256),  FieldElementT::UninitializedVector(256),
      FieldElementT::UninitializedVector(512),  FieldElementT::UninitializedVector(512),
      FieldElementT::UninitializedVector(1024), FieldElementT::UninitializedVector(1024),
      FieldElementT::UninitializedVector(1024), FieldElementT::UninitializedVector(1024),
      FieldElementT::UninitializedVector(1024), FieldElementT::UninitializedVector(2048),
      FieldElementT::UninitializedVector(2048), FieldElementT::UninitializedVector(2048),
      FieldElementT::UninitializedVector(4096), FieldElementT::UninitializedVector(4096),
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

  period = 64;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[7][i] = (point_powers[6][i & (63)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[8][i] = (point_powers[7][i & (127)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[9][i] = (point_powers[8][i & (255)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[10][i] = (point_powers[8][i & (255)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[11][i] = ((point_powers[8][i & (255)]) - (shifts[2])) *
                                   ((point_powers[8][i & (255)]) - (shifts[3])) *
                                   ((point_powers[8][i & (255)]) - (shifts[4])) *
                                   ((point_powers[8][i & (255)]) - (shifts[5])) *
                                   ((point_powers[8][i & (255)]) - (shifts[6])) *
                                   ((point_powers[8][i & (255)]) - (shifts[7])) *
                                   ((point_powers[8][i & (255)]) - (shifts[8])) *
                                   ((point_powers[8][i & (255)]) - (shifts[9])) *
                                   ((point_powers[8][i & (255)]) - (shifts[10])) *
                                   ((point_powers[8][i & (255)]) - (shifts[11])) *
                                   ((point_powers[8][i & (255)]) - (shifts[12])) *
                                   ((point_powers[8][i & (255)]) - (shifts[13])) *
                                   ((point_powers[8][i & (255)]) - (shifts[14])) *
                                   ((point_powers[8][i & (255)]) - (shifts[15])) *
                                   ((point_powers[8][i & (255)]) - (shifts[16])) *
                                   (precomp_domains[9][i & (256 - 1)]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[12][i] = (point_powers[9][i & (511)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[13][i] = (point_powers[9][i & (511)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[14][i] = (point_powers[10][i & (1023)]) - (shifts[17]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[15][i] = ((point_powers[10][i & (1023)]) - (shifts[18])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[19])) *
                                   (precomp_domains[14][i & (1024 - 1)]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[16][i] = ((point_powers[10][i & (1023)]) - (shifts[20])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[21])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[1])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[22])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[23])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[24])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[25])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[26])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[0])) *
                                   (precomp_domains[14][i & (1024 - 1)]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[17][i] = ((point_powers[10][i & (1023)]) - (shifts[27])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[28])) *
                                   ((point_powers[10][i & (1023)]) - (shifts[29])) *
                                   (precomp_domains[16][i & (1024 - 1)]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[18][i] = (point_powers[10][i & (1023)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 2048;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[19][i] = (point_powers[11][i & (2047)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 2048;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[20][i] = (point_powers[11][i & (2047)]) - (shifts[30]);
        }
      },
      period, kTaskSize);

  period = 2048;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[21][i] = (point_powers[11][i & (2047)]) - (shifts[19]);
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[22][i] = (point_powers[12][i & (4095)]) - (shifts[31]);
        }
      },
      period, kTaskSize);

  period = 4096;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[23][i] = (point_powers[12][i & (4095)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);
  return precomp_domains;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> CpuAirDefinition<FieldElementT, 12>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, [[maybe_unused]] const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 38, "shifts should contain 38 elements.");

  // domain0 = point^trace_length - 1.
  [[maybe_unused]] const FieldElementT& domain0 = precomp_domains[0];
  // domain1 = point^(trace_length / 2) - 1.
  [[maybe_unused]] const FieldElementT& domain1 = precomp_domains[1];
  // domain2 = point^(trace_length / 4) - 1.
  [[maybe_unused]] const FieldElementT& domain2 = precomp_domains[2];
  // domain3 = point^(trace_length / 8) - 1.
  [[maybe_unused]] const FieldElementT& domain3 = precomp_domains[3];
  // domain4 = point^(trace_length / 16) - gen^(15 * trace_length / 16).
  [[maybe_unused]] const FieldElementT& domain4 = precomp_domains[4];
  // domain5 = point^(trace_length / 16) - 1.
  [[maybe_unused]] const FieldElementT& domain5 = precomp_domains[5];
  // domain6 = point^(trace_length / 32) - 1.
  [[maybe_unused]] const FieldElementT& domain6 = precomp_domains[6];
  // domain7 = point^(trace_length / 64) - 1.
  [[maybe_unused]] const FieldElementT& domain7 = precomp_domains[7];
  // domain8 = point^(trace_length / 128) - 1.
  [[maybe_unused]] const FieldElementT& domain8 = precomp_domains[8];
  // domain9 = point^(trace_length / 256) - 1.
  [[maybe_unused]] const FieldElementT& domain9 = precomp_domains[9];
  // domain10 = point^(trace_length / 256) - gen^(3 * trace_length / 4).
  [[maybe_unused]] const FieldElementT& domain10 = precomp_domains[10];
  // domain11 = (point^(trace_length / 256) - gen^(trace_length / 64)) * (point^(trace_length / 256)
  // - gen^(trace_length / 32)) * (point^(trace_length / 256) - gen^(3 * trace_length / 64)) *
  // (point^(trace_length / 256) - gen^(trace_length / 16)) * (point^(trace_length / 256) - gen^(5 *
  // trace_length / 64)) * (point^(trace_length / 256) - gen^(3 * trace_length / 32)) *
  // (point^(trace_length / 256) - gen^(7 * trace_length / 64)) * (point^(trace_length / 256) -
  // gen^(trace_length / 8)) * (point^(trace_length / 256) - gen^(9 * trace_length / 64)) *
  // (point^(trace_length / 256) - gen^(5 * trace_length / 32)) * (point^(trace_length / 256) -
  // gen^(11 * trace_length / 64)) * (point^(trace_length / 256) - gen^(3 * trace_length / 16)) *
  // (point^(trace_length / 256) - gen^(13 * trace_length / 64)) * (point^(trace_length / 256) -
  // gen^(7 * trace_length / 32)) * (point^(trace_length / 256) - gen^(15 * trace_length / 64)) *
  // domain9.
  [[maybe_unused]] const FieldElementT& domain11 = precomp_domains[11];
  // domain12 = point^(trace_length / 512) - 1.
  [[maybe_unused]] const FieldElementT& domain12 = precomp_domains[12];
  // domain13 = point^(trace_length / 512) - gen^(3 * trace_length / 4).
  [[maybe_unused]] const FieldElementT& domain13 = precomp_domains[13];
  // domain14 = point^(trace_length / 1024) - gen^(31 * trace_length / 32).
  [[maybe_unused]] const FieldElementT& domain14 = precomp_domains[14];
  // domain15 = (point^(trace_length / 1024) - gen^(61 * trace_length / 64)) * (point^(trace_length
  // / 1024) - gen^(63 * trace_length / 64)) * domain14.
  [[maybe_unused]] const FieldElementT& domain15 = precomp_domains[15];
  // domain16 = (point^(trace_length / 1024) - gen^(11 * trace_length / 16)) * (point^(trace_length
  // / 1024) - gen^(23 * trace_length / 32)) * (point^(trace_length / 1024) - gen^(3 * trace_length
  // / 4)) * (point^(trace_length / 1024) - gen^(25 * trace_length / 32)) * (point^(trace_length /
  // 1024) - gen^(13 * trace_length / 16)) * (point^(trace_length / 1024) - gen^(27 * trace_length /
  // 32)) * (point^(trace_length / 1024) - gen^(7 * trace_length / 8)) * (point^(trace_length /
  // 1024) - gen^(29 * trace_length / 32)) * (point^(trace_length / 1024) - gen^(15 * trace_length /
  // 16)) * domain14.
  [[maybe_unused]] const FieldElementT& domain16 = precomp_domains[16];
  // domain17 = (point^(trace_length / 1024) - gen^(19 * trace_length / 32)) * (point^(trace_length
  // / 1024) - gen^(5 * trace_length / 8)) * (point^(trace_length / 1024) - gen^(21 * trace_length /
  // 32)) * domain16.
  [[maybe_unused]] const FieldElementT& domain17 = precomp_domains[17];
  // domain18 = point^(trace_length / 1024) - 1.
  [[maybe_unused]] const FieldElementT& domain18 = precomp_domains[18];
  // domain19 = point^(trace_length / 2048) - 1.
  [[maybe_unused]] const FieldElementT& domain19 = precomp_domains[19];
  // domain20 = point^(trace_length / 2048) - gen^(255 * trace_length / 256).
  [[maybe_unused]] const FieldElementT& domain20 = precomp_domains[20];
  // domain21 = point^(trace_length / 2048) - gen^(63 * trace_length / 64).
  [[maybe_unused]] const FieldElementT& domain21 = precomp_domains[21];
  // domain22 = point^(trace_length / 4096) - gen^(trace_length / 2).
  [[maybe_unused]] const FieldElementT& domain22 = precomp_domains[22];
  // domain23 = point^(trace_length / 4096) - 1.
  [[maybe_unused]] const FieldElementT& domain23 = precomp_domains[23];
  // domain24 = point - gen^(trace_length - 16).
  const FieldElementT& domain24 = (point) - (shifts[32]);
  // domain25 = point - 1.
  const FieldElementT& domain25 = (point) - (FieldElementT::One());
  // domain26 = point - gen^(trace_length - 2).
  const FieldElementT& domain26 = (point) - (shifts[33]);
  // domain27 = point - gen^(trace_length - 4).
  const FieldElementT& domain27 = (point) - (shifts[34]);
  // domain28 = point - gen^(trace_length - 4096).
  const FieldElementT& domain28 = (point) - (shifts[35]);
  // domain29 = point - gen^(trace_length - 256).
  const FieldElementT& domain29 = (point) - (shifts[36]);
  // domain30 = point - gen^(trace_length - 512).
  const FieldElementT& domain30 = (point) - (shifts[37]);

  ASSERT_VERIFIER(neighbors.size() == 192, "neighbors should contain 192 elements.");

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
  const FieldElementT& column1_row3 = neighbors[kColumn1Row3Neighbor];
  const FieldElementT& column1_row4 = neighbors[kColumn1Row4Neighbor];
  const FieldElementT& column1_row5 = neighbors[kColumn1Row5Neighbor];
  const FieldElementT& column1_row8 = neighbors[kColumn1Row8Neighbor];
  const FieldElementT& column1_row9 = neighbors[kColumn1Row9Neighbor];
  const FieldElementT& column1_row10 = neighbors[kColumn1Row10Neighbor];
  const FieldElementT& column1_row11 = neighbors[kColumn1Row11Neighbor];
  const FieldElementT& column1_row12 = neighbors[kColumn1Row12Neighbor];
  const FieldElementT& column1_row13 = neighbors[kColumn1Row13Neighbor];
  const FieldElementT& column1_row16 = neighbors[kColumn1Row16Neighbor];
  const FieldElementT& column1_row42 = neighbors[kColumn1Row42Neighbor];
  const FieldElementT& column1_row43 = neighbors[kColumn1Row43Neighbor];
  const FieldElementT& column1_row74 = neighbors[kColumn1Row74Neighbor];
  const FieldElementT& column1_row75 = neighbors[kColumn1Row75Neighbor];
  const FieldElementT& column1_row106 = neighbors[kColumn1Row106Neighbor];
  const FieldElementT& column1_row138 = neighbors[kColumn1Row138Neighbor];
  const FieldElementT& column1_row139 = neighbors[kColumn1Row139Neighbor];
  const FieldElementT& column1_row171 = neighbors[kColumn1Row171Neighbor];
  const FieldElementT& column1_row202 = neighbors[kColumn1Row202Neighbor];
  const FieldElementT& column1_row203 = neighbors[kColumn1Row203Neighbor];
  const FieldElementT& column1_row234 = neighbors[kColumn1Row234Neighbor];
  const FieldElementT& column1_row235 = neighbors[kColumn1Row235Neighbor];
  const FieldElementT& column1_row266 = neighbors[kColumn1Row266Neighbor];
  const FieldElementT& column1_row267 = neighbors[kColumn1Row267Neighbor];
  const FieldElementT& column1_row298 = neighbors[kColumn1Row298Neighbor];
  const FieldElementT& column1_row394 = neighbors[kColumn1Row394Neighbor];
  const FieldElementT& column1_row458 = neighbors[kColumn1Row458Neighbor];
  const FieldElementT& column1_row459 = neighbors[kColumn1Row459Neighbor];
  const FieldElementT& column1_row714 = neighbors[kColumn1Row714Neighbor];
  const FieldElementT& column1_row715 = neighbors[kColumn1Row715Neighbor];
  const FieldElementT& column1_row778 = neighbors[kColumn1Row778Neighbor];
  const FieldElementT& column1_row779 = neighbors[kColumn1Row779Neighbor];
  const FieldElementT& column1_row970 = neighbors[kColumn1Row970Neighbor];
  const FieldElementT& column1_row971 = neighbors[kColumn1Row971Neighbor];
  const FieldElementT& column1_row1034 = neighbors[kColumn1Row1034Neighbor];
  const FieldElementT& column1_row1035 = neighbors[kColumn1Row1035Neighbor];
  const FieldElementT& column1_row2058 = neighbors[kColumn1Row2058Neighbor];
  const FieldElementT& column1_row2059 = neighbors[kColumn1Row2059Neighbor];
  const FieldElementT& column1_row4106 = neighbors[kColumn1Row4106Neighbor];
  const FieldElementT& column2_row0 = neighbors[kColumn2Row0Neighbor];
  const FieldElementT& column2_row1 = neighbors[kColumn2Row1Neighbor];
  const FieldElementT& column2_row2 = neighbors[kColumn2Row2Neighbor];
  const FieldElementT& column2_row3 = neighbors[kColumn2Row3Neighbor];
  const FieldElementT& column3_row0 = neighbors[kColumn3Row0Neighbor];
  const FieldElementT& column3_row1 = neighbors[kColumn3Row1Neighbor];
  const FieldElementT& column3_row2 = neighbors[kColumn3Row2Neighbor];
  const FieldElementT& column3_row3 = neighbors[kColumn3Row3Neighbor];
  const FieldElementT& column3_row4 = neighbors[kColumn3Row4Neighbor];
  const FieldElementT& column3_row8 = neighbors[kColumn3Row8Neighbor];
  const FieldElementT& column3_row12 = neighbors[kColumn3Row12Neighbor];
  const FieldElementT& column3_row16 = neighbors[kColumn3Row16Neighbor];
  const FieldElementT& column3_row20 = neighbors[kColumn3Row20Neighbor];
  const FieldElementT& column3_row24 = neighbors[kColumn3Row24Neighbor];
  const FieldElementT& column3_row28 = neighbors[kColumn3Row28Neighbor];
  const FieldElementT& column3_row32 = neighbors[kColumn3Row32Neighbor];
  const FieldElementT& column3_row36 = neighbors[kColumn3Row36Neighbor];
  const FieldElementT& column3_row40 = neighbors[kColumn3Row40Neighbor];
  const FieldElementT& column3_row44 = neighbors[kColumn3Row44Neighbor];
  const FieldElementT& column3_row48 = neighbors[kColumn3Row48Neighbor];
  const FieldElementT& column3_row52 = neighbors[kColumn3Row52Neighbor];
  const FieldElementT& column3_row56 = neighbors[kColumn3Row56Neighbor];
  const FieldElementT& column3_row60 = neighbors[kColumn3Row60Neighbor];
  const FieldElementT& column3_row64 = neighbors[kColumn3Row64Neighbor];
  const FieldElementT& column3_row66 = neighbors[kColumn3Row66Neighbor];
  const FieldElementT& column3_row128 = neighbors[kColumn3Row128Neighbor];
  const FieldElementT& column3_row130 = neighbors[kColumn3Row130Neighbor];
  const FieldElementT& column3_row176 = neighbors[kColumn3Row176Neighbor];
  const FieldElementT& column3_row180 = neighbors[kColumn3Row180Neighbor];
  const FieldElementT& column3_row184 = neighbors[kColumn3Row184Neighbor];
  const FieldElementT& column3_row188 = neighbors[kColumn3Row188Neighbor];
  const FieldElementT& column3_row192 = neighbors[kColumn3Row192Neighbor];
  const FieldElementT& column3_row194 = neighbors[kColumn3Row194Neighbor];
  const FieldElementT& column3_row240 = neighbors[kColumn3Row240Neighbor];
  const FieldElementT& column3_row244 = neighbors[kColumn3Row244Neighbor];
  const FieldElementT& column3_row248 = neighbors[kColumn3Row248Neighbor];
  const FieldElementT& column3_row252 = neighbors[kColumn3Row252Neighbor];
  const FieldElementT& column4_row0 = neighbors[kColumn4Row0Neighbor];
  const FieldElementT& column4_row1 = neighbors[kColumn4Row1Neighbor];
  const FieldElementT& column4_row2 = neighbors[kColumn4Row2Neighbor];
  const FieldElementT& column4_row3 = neighbors[kColumn4Row3Neighbor];
  const FieldElementT& column4_row4 = neighbors[kColumn4Row4Neighbor];
  const FieldElementT& column4_row5 = neighbors[kColumn4Row5Neighbor];
  const FieldElementT& column4_row6 = neighbors[kColumn4Row6Neighbor];
  const FieldElementT& column4_row7 = neighbors[kColumn4Row7Neighbor];
  const FieldElementT& column4_row8 = neighbors[kColumn4Row8Neighbor];
  const FieldElementT& column4_row9 = neighbors[kColumn4Row9Neighbor];
  const FieldElementT& column4_row11 = neighbors[kColumn4Row11Neighbor];
  const FieldElementT& column4_row12 = neighbors[kColumn4Row12Neighbor];
  const FieldElementT& column4_row13 = neighbors[kColumn4Row13Neighbor];
  const FieldElementT& column4_row44 = neighbors[kColumn4Row44Neighbor];
  const FieldElementT& column4_row76 = neighbors[kColumn4Row76Neighbor];
  const FieldElementT& column4_row108 = neighbors[kColumn4Row108Neighbor];
  const FieldElementT& column4_row140 = neighbors[kColumn4Row140Neighbor];
  const FieldElementT& column4_row172 = neighbors[kColumn4Row172Neighbor];
  const FieldElementT& column4_row204 = neighbors[kColumn4Row204Neighbor];
  const FieldElementT& column4_row236 = neighbors[kColumn4Row236Neighbor];
  const FieldElementT& column4_row1539 = neighbors[kColumn4Row1539Neighbor];
  const FieldElementT& column4_row1547 = neighbors[kColumn4Row1547Neighbor];
  const FieldElementT& column4_row1571 = neighbors[kColumn4Row1571Neighbor];
  const FieldElementT& column4_row1579 = neighbors[kColumn4Row1579Neighbor];
  const FieldElementT& column4_row2011 = neighbors[kColumn4Row2011Neighbor];
  const FieldElementT& column4_row2019 = neighbors[kColumn4Row2019Neighbor];
  const FieldElementT& column4_row2041 = neighbors[kColumn4Row2041Neighbor];
  const FieldElementT& column4_row2045 = neighbors[kColumn4Row2045Neighbor];
  const FieldElementT& column4_row2047 = neighbors[kColumn4Row2047Neighbor];
  const FieldElementT& column4_row2049 = neighbors[kColumn4Row2049Neighbor];
  const FieldElementT& column4_row2051 = neighbors[kColumn4Row2051Neighbor];
  const FieldElementT& column4_row2053 = neighbors[kColumn4Row2053Neighbor];
  const FieldElementT& column4_row4089 = neighbors[kColumn4Row4089Neighbor];
  const FieldElementT& column5_row0 = neighbors[kColumn5Row0Neighbor];
  const FieldElementT& column5_row1 = neighbors[kColumn5Row1Neighbor];
  const FieldElementT& column5_row2 = neighbors[kColumn5Row2Neighbor];
  const FieldElementT& column5_row4 = neighbors[kColumn5Row4Neighbor];
  const FieldElementT& column5_row6 = neighbors[kColumn5Row6Neighbor];
  const FieldElementT& column5_row8 = neighbors[kColumn5Row8Neighbor];
  const FieldElementT& column5_row9 = neighbors[kColumn5Row9Neighbor];
  const FieldElementT& column5_row10 = neighbors[kColumn5Row10Neighbor];
  const FieldElementT& column5_row12 = neighbors[kColumn5Row12Neighbor];
  const FieldElementT& column5_row14 = neighbors[kColumn5Row14Neighbor];
  const FieldElementT& column5_row16 = neighbors[kColumn5Row16Neighbor];
  const FieldElementT& column5_row17 = neighbors[kColumn5Row17Neighbor];
  const FieldElementT& column5_row22 = neighbors[kColumn5Row22Neighbor];
  const FieldElementT& column5_row24 = neighbors[kColumn5Row24Neighbor];
  const FieldElementT& column5_row25 = neighbors[kColumn5Row25Neighbor];
  const FieldElementT& column5_row30 = neighbors[kColumn5Row30Neighbor];
  const FieldElementT& column5_row33 = neighbors[kColumn5Row33Neighbor];
  const FieldElementT& column5_row38 = neighbors[kColumn5Row38Neighbor];
  const FieldElementT& column5_row41 = neighbors[kColumn5Row41Neighbor];
  const FieldElementT& column5_row46 = neighbors[kColumn5Row46Neighbor];
  const FieldElementT& column5_row49 = neighbors[kColumn5Row49Neighbor];
  const FieldElementT& column5_row54 = neighbors[kColumn5Row54Neighbor];
  const FieldElementT& column5_row57 = neighbors[kColumn5Row57Neighbor];
  const FieldElementT& column5_row65 = neighbors[kColumn5Row65Neighbor];
  const FieldElementT& column5_row73 = neighbors[kColumn5Row73Neighbor];
  const FieldElementT& column5_row81 = neighbors[kColumn5Row81Neighbor];
  const FieldElementT& column5_row89 = neighbors[kColumn5Row89Neighbor];
  const FieldElementT& column5_row97 = neighbors[kColumn5Row97Neighbor];
  const FieldElementT& column5_row105 = neighbors[kColumn5Row105Neighbor];
  const FieldElementT& column5_row137 = neighbors[kColumn5Row137Neighbor];
  const FieldElementT& column5_row169 = neighbors[kColumn5Row169Neighbor];
  const FieldElementT& column5_row201 = neighbors[kColumn5Row201Neighbor];
  const FieldElementT& column5_row393 = neighbors[kColumn5Row393Neighbor];
  const FieldElementT& column5_row409 = neighbors[kColumn5Row409Neighbor];
  const FieldElementT& column5_row425 = neighbors[kColumn5Row425Neighbor];
  const FieldElementT& column5_row457 = neighbors[kColumn5Row457Neighbor];
  const FieldElementT& column5_row473 = neighbors[kColumn5Row473Neighbor];
  const FieldElementT& column5_row489 = neighbors[kColumn5Row489Neighbor];
  const FieldElementT& column5_row521 = neighbors[kColumn5Row521Neighbor];
  const FieldElementT& column5_row553 = neighbors[kColumn5Row553Neighbor];
  const FieldElementT& column5_row585 = neighbors[kColumn5Row585Neighbor];
  const FieldElementT& column5_row609 = neighbors[kColumn5Row609Neighbor];
  const FieldElementT& column5_row625 = neighbors[kColumn5Row625Neighbor];
  const FieldElementT& column5_row641 = neighbors[kColumn5Row641Neighbor];
  const FieldElementT& column5_row657 = neighbors[kColumn5Row657Neighbor];
  const FieldElementT& column5_row673 = neighbors[kColumn5Row673Neighbor];
  const FieldElementT& column5_row689 = neighbors[kColumn5Row689Neighbor];
  const FieldElementT& column5_row905 = neighbors[kColumn5Row905Neighbor];
  const FieldElementT& column5_row921 = neighbors[kColumn5Row921Neighbor];
  const FieldElementT& column5_row937 = neighbors[kColumn5Row937Neighbor];
  const FieldElementT& column5_row969 = neighbors[kColumn5Row969Neighbor];
  const FieldElementT& column5_row982 = neighbors[kColumn5Row982Neighbor];
  const FieldElementT& column5_row985 = neighbors[kColumn5Row985Neighbor];
  const FieldElementT& column5_row998 = neighbors[kColumn5Row998Neighbor];
  const FieldElementT& column5_row1001 = neighbors[kColumn5Row1001Neighbor];
  const FieldElementT& column5_row1014 = neighbors[kColumn5Row1014Neighbor];
  const FieldElementT& column6_inter1_row0 = neighbors[kColumn6Inter1Row0Neighbor];
  const FieldElementT& column6_inter1_row1 = neighbors[kColumn6Inter1Row1Neighbor];
  const FieldElementT& column6_inter1_row2 = neighbors[kColumn6Inter1Row2Neighbor];
  const FieldElementT& column6_inter1_row3 = neighbors[kColumn6Inter1Row3Neighbor];
  const FieldElementT& column7_inter1_row0 = neighbors[kColumn7Inter1Row0Neighbor];
  const FieldElementT& column7_inter1_row1 = neighbors[kColumn7Inter1Row1Neighbor];
  const FieldElementT& column7_inter1_row2 = neighbors[kColumn7Inter1Row2Neighbor];
  const FieldElementT& column7_inter1_row5 = neighbors[kColumn7Inter1Row5Neighbor];

  ASSERT_VERIFIER(periodic_columns.size() == 7, "periodic_columns should contain 7 elements.");
  const FieldElementT& pedersen__points__x = periodic_columns[kPedersenPointsXPeriodicColumn];
  const FieldElementT& pedersen__points__y = periodic_columns[kPedersenPointsYPeriodicColumn];
  const FieldElementT& poseidon__poseidon__full_round_key0 =
      periodic_columns[kPoseidonPoseidonFullRoundKey0PeriodicColumn];
  const FieldElementT& poseidon__poseidon__full_round_key1 =
      periodic_columns[kPoseidonPoseidonFullRoundKey1PeriodicColumn];
  const FieldElementT& poseidon__poseidon__full_round_key2 =
      periodic_columns[kPoseidonPoseidonFullRoundKey2PeriodicColumn];
  const FieldElementT& poseidon__poseidon__partial_round_key0 =
      periodic_columns[kPoseidonPoseidonPartialRoundKey0PeriodicColumn];
  const FieldElementT& poseidon__poseidon__partial_round_key1 =
      periodic_columns[kPoseidonPoseidonPartialRoundKey1PeriodicColumn];

  const FieldElementT cpu__decode__opcode_range_check__bit_0 =
      (column0_row0) - ((column0_row1) + (column0_row1));
  const FieldElementT cpu__decode__opcode_range_check__bit_2 =
      (column0_row2) - ((column0_row3) + (column0_row3));
  const FieldElementT cpu__decode__opcode_range_check__bit_4 =
      (column0_row4) - ((column0_row5) + (column0_row5));
  const FieldElementT cpu__decode__opcode_range_check__bit_3 =
      (column0_row3) - ((column0_row4) + (column0_row4));
  const FieldElementT cpu__decode__flag_op1_base_op0_0 =
      (FieldElementT::One()) -
      (((cpu__decode__opcode_range_check__bit_2) + (cpu__decode__opcode_range_check__bit_4)) +
       (cpu__decode__opcode_range_check__bit_3));
  const FieldElementT cpu__decode__opcode_range_check__bit_5 =
      (column0_row5) - ((column0_row6) + (column0_row6));
  const FieldElementT cpu__decode__opcode_range_check__bit_6 =
      (column0_row6) - ((column0_row7) + (column0_row7));
  const FieldElementT cpu__decode__opcode_range_check__bit_9 =
      (column0_row9) - ((column0_row10) + (column0_row10));
  const FieldElementT cpu__decode__flag_res_op1_0 =
      (FieldElementT::One()) -
      (((cpu__decode__opcode_range_check__bit_5) + (cpu__decode__opcode_range_check__bit_6)) +
       (cpu__decode__opcode_range_check__bit_9));
  const FieldElementT cpu__decode__opcode_range_check__bit_7 =
      (column0_row7) - ((column0_row8) + (column0_row8));
  const FieldElementT cpu__decode__opcode_range_check__bit_8 =
      (column0_row8) - ((column0_row9) + (column0_row9));
  const FieldElementT cpu__decode__flag_pc_update_regular_0 =
      (FieldElementT::One()) -
      (((cpu__decode__opcode_range_check__bit_7) + (cpu__decode__opcode_range_check__bit_8)) +
       (cpu__decode__opcode_range_check__bit_9));
  const FieldElementT cpu__decode__opcode_range_check__bit_12 =
      (column0_row12) - ((column0_row13) + (column0_row13));
  const FieldElementT cpu__decode__opcode_range_check__bit_13 =
      (column0_row13) - ((column0_row14) + (column0_row14));
  const FieldElementT cpu__decode__fp_update_regular_0 =
      (FieldElementT::One()) -
      ((cpu__decode__opcode_range_check__bit_12) + (cpu__decode__opcode_range_check__bit_13));
  const FieldElementT cpu__decode__opcode_range_check__bit_1 =
      (column0_row1) - ((column0_row2) + (column0_row2));
  const FieldElementT npc_reg_0 =
      ((column1_row0) + (cpu__decode__opcode_range_check__bit_2)) + (FieldElementT::One());
  const FieldElementT cpu__decode__opcode_range_check__bit_10 =
      (column0_row10) - ((column0_row11) + (column0_row11));
  const FieldElementT cpu__decode__opcode_range_check__bit_11 =
      (column0_row11) - ((column0_row12) + (column0_row12));
  const FieldElementT cpu__decode__opcode_range_check__bit_14 =
      (column0_row14) - ((column0_row15) + (column0_row15));
  const FieldElementT memory__address_diff_0 = (column2_row2) - (column2_row0);
  const FieldElementT range_check16__diff_0 = (column4_row6) - (column4_row2);
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_0 =
      (column4_row3) - ((column4_row11) + (column4_row11));
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash0__ec_subset_sum__bit_0);
  const FieldElementT range_check_builtin__value0_0 = column4_row12;
  const FieldElementT range_check_builtin__value1_0 =
      ((range_check_builtin__value0_0) * (offset_size_)) + (column4_row44);
  const FieldElementT range_check_builtin__value2_0 =
      ((range_check_builtin__value1_0) * (offset_size_)) + (column4_row76);
  const FieldElementT range_check_builtin__value3_0 =
      ((range_check_builtin__value2_0) * (offset_size_)) + (column4_row108);
  const FieldElementT range_check_builtin__value4_0 =
      ((range_check_builtin__value3_0) * (offset_size_)) + (column4_row140);
  const FieldElementT range_check_builtin__value5_0 =
      ((range_check_builtin__value4_0) * (offset_size_)) + (column4_row172);
  const FieldElementT range_check_builtin__value6_0 =
      ((range_check_builtin__value5_0) * (offset_size_)) + (column4_row204);
  const FieldElementT range_check_builtin__value7_0 =
      ((range_check_builtin__value6_0) * (offset_size_)) + (column4_row236);
  const FieldElementT bitwise__sum_var_0_0 =
      (((((((column3_row0) + ((column3_row4) * (FieldElementT::ConstexprFromBigInt(0x2_Z)))) +
           ((column3_row8) * (FieldElementT::ConstexprFromBigInt(0x4_Z)))) +
          ((column3_row12) * (FieldElementT::ConstexprFromBigInt(0x8_Z)))) +
         ((column3_row16) * (FieldElementT::ConstexprFromBigInt(0x10000000000000000_Z)))) +
        ((column3_row20) * (FieldElementT::ConstexprFromBigInt(0x20000000000000000_Z)))) +
       ((column3_row24) * (FieldElementT::ConstexprFromBigInt(0x40000000000000000_Z)))) +
      ((column3_row28) * (FieldElementT::ConstexprFromBigInt(0x80000000000000000_Z)));
  const FieldElementT bitwise__sum_var_8_0 =
      ((((((((column3_row32) *
             (FieldElementT::ConstexprFromBigInt(0x100000000000000000000000000000000_Z))) +
            ((column3_row36) *
             (FieldElementT::ConstexprFromBigInt(0x200000000000000000000000000000000_Z)))) +
           ((column3_row40) *
            (FieldElementT::ConstexprFromBigInt(0x400000000000000000000000000000000_Z)))) +
          ((column3_row44) *
           (FieldElementT::ConstexprFromBigInt(0x800000000000000000000000000000000_Z)))) +
         ((column3_row48) * (FieldElementT::ConstexprFromBigInt(
                                0x1000000000000000000000000000000000000000000000000_Z)))) +
        ((column3_row52) * (FieldElementT::ConstexprFromBigInt(
                               0x2000000000000000000000000000000000000000000000000_Z)))) +
       ((column3_row56) * (FieldElementT::ConstexprFromBigInt(
                              0x4000000000000000000000000000000000000000000000000_Z)))) +
      ((column3_row60) *
       (FieldElementT::ConstexprFromBigInt(0x8000000000000000000000000000000000000000000000000_Z)));
  const FieldElementT poseidon__poseidon__full_rounds_state0_cubed_0 =
      (column5_row9) * (column5_row105);
  const FieldElementT poseidon__poseidon__full_rounds_state1_cubed_0 =
      (column5_row73) * (column5_row25);
  const FieldElementT poseidon__poseidon__full_rounds_state2_cubed_0 =
      (column5_row41) * (column5_row89);
  const FieldElementT poseidon__poseidon__full_rounds_state0_cubed_7 =
      (column5_row905) * (column5_row1001);
  const FieldElementT poseidon__poseidon__full_rounds_state1_cubed_7 =
      (column5_row969) * (column5_row921);
  const FieldElementT poseidon__poseidon__full_rounds_state2_cubed_7 =
      (column5_row937) * (column5_row985);
  const FieldElementT poseidon__poseidon__full_rounds_state0_cubed_3 =
      (column5_row393) * (column5_row489);
  const FieldElementT poseidon__poseidon__full_rounds_state1_cubed_3 =
      (column5_row457) * (column5_row409);
  const FieldElementT poseidon__poseidon__full_rounds_state2_cubed_3 =
      (column5_row425) * (column5_row473);
  const FieldElementT poseidon__poseidon__partial_rounds_state0_cubed_0 =
      (column5_row6) * (column5_row14);
  const FieldElementT poseidon__poseidon__partial_rounds_state0_cubed_1 =
      (column5_row22) * (column5_row30);
  const FieldElementT poseidon__poseidon__partial_rounds_state0_cubed_2 =
      (column5_row38) * (column5_row46);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_0 =
      (column5_row1) * (column5_row17);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_1 =
      (column5_row33) * (column5_row49);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_2 =
      (column5_row65) * (column5_row81);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_19 =
      (column5_row609) * (column5_row625);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_20 =
      (column5_row641) * (column5_row657);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_21 =
      (column5_row673) * (column5_row689);
  FractionFieldElement<FieldElementT> res(FieldElementT::Zero());
  {
    // Compute a sum of constraints with denominator = domain0.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain4.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/decode/opcode_range_check/bit:
        const FieldElementT constraint =
            ((cpu__decode__opcode_range_check__bit_0) * (cpu__decode__opcode_range_check__bit_0)) -
            (cpu__decode__opcode_range_check__bit_0);
        inner_sum += random_coefficients[0] * constraint;
      }
      outer_sum += inner_sum * domain4;
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
        // Constraint expression for cpu/decode/opcode_range_check/zero:
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
        // Constraint expression for cpu/decode/opcode_range_check_input:
        const FieldElementT constraint =
            (column1_row1) -
            (((((((column0_row0) * (offset_size_)) + (column4_row4)) * (offset_size_)) +
               (column4_row8)) *
              (offset_size_)) +
             (column4_row0));
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
            ((column1_row8) + (half_offset_size_)) -
            ((((cpu__decode__opcode_range_check__bit_0) * (column5_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_range_check__bit_0)) *
               (column5_row0))) +
             (column4_row0));
        inner_sum += random_coefficients[7] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem0_addr:
        const FieldElementT constraint =
            ((column1_row4) + (half_offset_size_)) -
            ((((cpu__decode__opcode_range_check__bit_1) * (column5_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_range_check__bit_1)) *
               (column5_row0))) +
             (column4_row8));
        inner_sum += random_coefficients[8] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem1_addr:
        const FieldElementT constraint =
            ((column1_row12) + (half_offset_size_)) -
            ((((((cpu__decode__opcode_range_check__bit_2) * (column1_row0)) +
                ((cpu__decode__opcode_range_check__bit_4) * (column5_row0))) +
               ((cpu__decode__opcode_range_check__bit_3) * (column5_row8))) +
              ((cpu__decode__flag_op1_base_op0_0) * (column1_row5))) +
             (column4_row4));
        inner_sum += random_coefficients[9] * constraint;
      }
      {
        // Constraint expression for cpu/operands/ops_mul:
        const FieldElementT constraint = (column5_row4) - ((column1_row5) * (column1_row13));
        inner_sum += random_coefficients[10] * constraint;
      }
      {
        // Constraint expression for cpu/operands/res:
        const FieldElementT constraint =
            (((FieldElementT::One()) - (cpu__decode__opcode_range_check__bit_9)) *
             (column5_row12)) -
            ((((cpu__decode__opcode_range_check__bit_5) * ((column1_row5) + (column1_row13))) +
              ((cpu__decode__opcode_range_check__bit_6) * (column5_row4))) +
             ((cpu__decode__flag_res_op1_0) * (column1_row13)));
        inner_sum += random_coefficients[11] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_fp:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_12) * ((column1_row9) - (column5_row8));
        inner_sum += random_coefficients[18] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_pc:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_12) *
            ((column1_row5) - (((column1_row0) + (cpu__decode__opcode_range_check__bit_2)) +
                               (FieldElementT::One())));
        inner_sum += random_coefficients[19] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off0:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_12) * ((column4_row0) - (half_offset_size_));
        inner_sum += random_coefficients[20] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off1:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_12) *
            ((column4_row8) - ((half_offset_size_) + (FieldElementT::One())));
        inner_sum += random_coefficients[21] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/flags:
        const FieldElementT constraint = (cpu__decode__opcode_range_check__bit_12) *
                                         (((((cpu__decode__opcode_range_check__bit_12) +
                                             (cpu__decode__opcode_range_check__bit_12)) +
                                            (FieldElementT::One())) +
                                           (FieldElementT::One())) -
                                          (((cpu__decode__opcode_range_check__bit_0) +
                                            (cpu__decode__opcode_range_check__bit_1)) +
                                           (FieldElementT::ConstexprFromBigInt(0x4_Z))));
        inner_sum += random_coefficients[22] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/off0:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_13) *
            (((column4_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z))) - (half_offset_size_));
        inner_sum += random_coefficients[23] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/off2:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_13) *
            (((column4_row4) + (FieldElementT::One())) - (half_offset_size_));
        inner_sum += random_coefficients[24] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/flags:
        const FieldElementT constraint = (cpu__decode__opcode_range_check__bit_13) *
                                         (((((cpu__decode__opcode_range_check__bit_7) +
                                             (cpu__decode__opcode_range_check__bit_0)) +
                                            (cpu__decode__opcode_range_check__bit_3)) +
                                           (cpu__decode__flag_res_op1_0)) -
                                          (FieldElementT::ConstexprFromBigInt(0x4_Z)));
        inner_sum += random_coefficients[25] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/assert_eq/assert_eq:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_14) * ((column1_row9) - (column5_row12));
        inner_sum += random_coefficients[26] * constraint;
      }
      {
        // Constraint expression for public_memory_addr_zero:
        const FieldElementT constraint = column1_row2;
        inner_sum += random_coefficients[39] * constraint;
      }
      {
        // Constraint expression for public_memory_value_zero:
        const FieldElementT constraint = column1_row3;
        inner_sum += random_coefficients[40] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/partial_rounds_state0_squaring:
        const FieldElementT constraint = ((column5_row6) * (column5_row6)) - (column5_row14);
        inner_sum += random_coefficients[102] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain24.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp0:
        const FieldElementT constraint =
            (column5_row2) - ((cpu__decode__opcode_range_check__bit_9) * (column1_row9));
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
            ((((FieldElementT::One()) - (cpu__decode__opcode_range_check__bit_9)) *
              (column1_row16)) +
             ((column5_row2) * ((column1_row16) - ((column1_row0) + (column1_row13))))) -
            ((((cpu__decode__flag_pc_update_regular_0) * (npc_reg_0)) +
              ((cpu__decode__opcode_range_check__bit_7) * (column5_row12))) +
             ((cpu__decode__opcode_range_check__bit_8) * ((column1_row0) + (column5_row12))));
        inner_sum += random_coefficients[14] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_positive:
        const FieldElementT constraint =
            ((column5_row10) - (cpu__decode__opcode_range_check__bit_9)) *
            ((column1_row16) - (npc_reg_0));
        inner_sum += random_coefficients[15] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_ap/ap_update:
        const FieldElementT constraint =
            (column5_row16) -
            ((((column5_row0) + ((cpu__decode__opcode_range_check__bit_10) * (column5_row12))) +
              (cpu__decode__opcode_range_check__bit_11)) +
             ((cpu__decode__opcode_range_check__bit_12) *
              (FieldElementT::ConstexprFromBigInt(0x2_Z))));
        inner_sum += random_coefficients[16] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_fp/fp_update:
        const FieldElementT constraint =
            (column5_row24) - ((((cpu__decode__fp_update_regular_0) * (column5_row8)) +
                                ((cpu__decode__opcode_range_check__bit_13) * (column1_row9))) +
                               ((cpu__decode__opcode_range_check__bit_12) *
                                ((column5_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z)))));
        inner_sum += random_coefficients[17] * constraint;
      }
      outer_sum += inner_sum * domain24;
    }

    {
      // Compute a sum of constraints with numerator = domain15.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/partial_round0:
        const FieldElementT constraint =
            (column5_row54) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__partial_rounds_state0_cubed_0)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column5_row22))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state0_cubed_1))) +
                (column5_row38)) +
               (column5_row38)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state0_cubed_2))) +
             (poseidon__poseidon__partial_round_key0));
        inner_sum += random_coefficients[119] * constraint;
      }
      outer_sum += inner_sum * domain15;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain5);
  }

  {
    // Compute a sum of constraints with denominator = domain25.
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
        const FieldElementT constraint = (column1_row0) - (initial_pc_);
        inner_sum += random_coefficients[29] * constraint;
      }
      {
        // Constraint expression for memory/multi_column_perm/perm/init0:
        const FieldElementT constraint =
            (((((memory__multi_column_perm__perm__interaction_elm_) -
                ((column2_row0) +
                 ((memory__multi_column_perm__hash_interaction_elm0_) * (column2_row1)))) *
               (column6_inter1_row0)) +
              (column1_row0)) +
             ((memory__multi_column_perm__hash_interaction_elm0_) * (column1_row1))) -
            (memory__multi_column_perm__perm__interaction_elm_);
        inner_sum += random_coefficients[33] * constraint;
      }
      {
        // Constraint expression for memory/initial_addr:
        const FieldElementT constraint = (column2_row0) - (FieldElementT::One());
        inner_sum += random_coefficients[38] * constraint;
      }
      {
        // Constraint expression for range_check16/perm/init0:
        const FieldElementT constraint =
            ((((range_check16__perm__interaction_elm_) - (column4_row2)) * (column7_inter1_row1)) +
             (column4_row0)) -
            (range_check16__perm__interaction_elm_);
        inner_sum += random_coefficients[41] * constraint;
      }
      {
        // Constraint expression for range_check16/minimum:
        const FieldElementT constraint = (column4_row2) - (range_check_min_);
        inner_sum += random_coefficients[45] * constraint;
      }
      {
        // Constraint expression for diluted_check/permutation/init0:
        const FieldElementT constraint =
            ((((diluted_check__permutation__interaction_elm_) - (column3_row1)) *
              (column7_inter1_row0)) +
             (column3_row0)) -
            (diluted_check__permutation__interaction_elm_);
        inner_sum += random_coefficients[47] * constraint;
      }
      {
        // Constraint expression for diluted_check/init:
        const FieldElementT constraint = (column6_inter1_row1) - (FieldElementT::One());
        inner_sum += random_coefficients[50] * constraint;
      }
      {
        // Constraint expression for diluted_check/first_element:
        const FieldElementT constraint = (column3_row1) - (diluted_check__first_elm_);
        inner_sum += random_coefficients[51] * constraint;
      }
      {
        // Constraint expression for pedersen/init_addr:
        const FieldElementT constraint = (column1_row10) - (initial_pedersen_addr_);
        inner_sum += random_coefficients[74] * constraint;
      }
      {
        // Constraint expression for range_check_builtin/init_addr:
        const FieldElementT constraint = (column1_row138) - (initial_range_check_addr_);
        inner_sum += random_coefficients[81] * constraint;
      }
      {
        // Constraint expression for bitwise/init_var_pool_addr:
        const FieldElementT constraint = (column1_row42) - (initial_bitwise_addr_);
        inner_sum += random_coefficients[82] * constraint;
      }
      {
        // Constraint expression for poseidon/param_0/init_input_output_addr:
        const FieldElementT constraint = (column1_row266) - (initial_poseidon_addr_);
        inner_sum += random_coefficients[93] * constraint;
      }
      {
        // Constraint expression for poseidon/param_1/init_input_output_addr:
        const FieldElementT constraint =
            (column1_row202) - ((initial_poseidon_addr_) + (FieldElementT::One()));
        inner_sum += random_coefficients[95] * constraint;
      }
      {
        // Constraint expression for poseidon/param_2/init_input_output_addr:
        const FieldElementT constraint =
            (column1_row458) -
            ((initial_poseidon_addr_) + (FieldElementT::ConstexprFromBigInt(0x2_Z)));
        inner_sum += random_coefficients[97] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain25);
  }

  {
    // Compute a sum of constraints with denominator = domain24.
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
        const FieldElementT constraint = (column1_row0) - (final_pc_);
        inner_sum += random_coefficients[32] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain24);
  }

  {
    // Compute a sum of constraints with denominator = domain1.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain26.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/step0:
        const FieldElementT constraint =
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column2_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column2_row3)))) *
             (column6_inter1_row2)) -
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column1_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column1_row3)))) *
             (column6_inter1_row0));
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
            ((memory__address_diff_0) - (FieldElementT::One())) * ((column2_row1) - (column2_row3));
        inner_sum += random_coefficients[37] * constraint;
      }
      {
        // Constraint expression for diluted_check/permutation/step0:
        const FieldElementT constraint =
            (((diluted_check__permutation__interaction_elm_) - (column3_row3)) *
             (column7_inter1_row2)) -
            (((diluted_check__permutation__interaction_elm_) - (column3_row2)) *
             (column7_inter1_row0));
        inner_sum += random_coefficients[48] * constraint;
      }
      {
        // Constraint expression for diluted_check/step:
        const FieldElementT constraint =
            (column6_inter1_row3) -
            (((column6_inter1_row1) *
              ((FieldElementT::One()) +
               ((diluted_check__interaction_z_) * ((column3_row3) - (column3_row1))))) +
             (((diluted_check__interaction_alpha_) * ((column3_row3) - (column3_row1))) *
              ((column3_row3) - (column3_row1))));
        inner_sum += random_coefficients[52] * constraint;
      }
      outer_sum += inner_sum * domain26;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain1);
  }

  {
    // Compute a sum of constraints with denominator = domain26.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/last:
        const FieldElementT constraint =
            (column6_inter1_row0) - (memory__multi_column_perm__perm__public_memory_prod_);
        inner_sum += random_coefficients[35] * constraint;
      }
      {
        // Constraint expression for diluted_check/permutation/last:
        const FieldElementT constraint =
            (column7_inter1_row0) - (diluted_check__permutation__public_memory_prod_);
        inner_sum += random_coefficients[49] * constraint;
      }
      {
        // Constraint expression for diluted_check/last:
        const FieldElementT constraint = (column6_inter1_row1) - (diluted_check__final_cum_val_);
        inner_sum += random_coefficients[53] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain26);
  }

  {
    // Compute a sum of constraints with denominator = domain2.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain27.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for range_check16/perm/step0:
        const FieldElementT constraint =
            (((range_check16__perm__interaction_elm_) - (column4_row6)) * (column7_inter1_row5)) -
            (((range_check16__perm__interaction_elm_) - (column4_row4)) * (column7_inter1_row1));
        inner_sum += random_coefficients[42] * constraint;
      }
      {
        // Constraint expression for range_check16/diff_is_bit:
        const FieldElementT constraint =
            ((range_check16__diff_0) * (range_check16__diff_0)) - (range_check16__diff_0);
        inner_sum += random_coefficients[44] * constraint;
      }
      outer_sum += inner_sum * domain27;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain2);
  }

  {
    // Compute a sum of constraints with denominator = domain27.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for range_check16/perm/last:
        const FieldElementT constraint =
            (column7_inter1_row1) - (range_check16__perm__public_memory_prod_);
        inner_sum += random_coefficients[43] * constraint;
      }
      {
        // Constraint expression for range_check16/maximum:
        const FieldElementT constraint = (column4_row2) - (range_check_max_);
        inner_sum += random_coefficients[46] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain27);
  }

  {
    // Compute a sum of constraints with denominator = domain19.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column5_row57) * ((column4_row3) - ((column4_row11) + (column4_row11)));
        inner_sum += random_coefficients[54] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column5_row57) *
            ((column4_row11) - ((FieldElementT::ConstexprFromBigInt(
                                    0x800000000000000000000000000000000000000000000000_Z)) *
                                (column4_row1539)));
        inner_sum += random_coefficients[55] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column5_row57) -
            ((column4_row2047) * ((column4_row1539) - ((column4_row1547) + (column4_row1547))));
        inner_sum += random_coefficients[56] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column4_row2047) *
            ((column4_row1547) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column4_row1571)));
        inner_sum += random_coefficients[57] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column4_row2047) - (((column4_row2011) - ((column4_row2019) + (column4_row2019))) *
                                 ((column4_row1571) - ((column4_row1579) + (column4_row1579))));
        inner_sum += random_coefficients[58] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column4_row2011) - ((column4_row2019) + (column4_row2019))) *
            ((column4_row1579) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column4_row2011)));
        inner_sum += random_coefficients[59] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain22.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/copy_point/x:
        const FieldElementT constraint = (column4_row2049) - (column4_row2041);
        inner_sum += random_coefficients[68] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/copy_point/y:
        const FieldElementT constraint = (column4_row2053) - (column4_row2045);
        inner_sum += random_coefficients[69] * constraint;
      }
      outer_sum += inner_sum * domain22;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain19);
  }

  {
    // Compute a sum of constraints with denominator = domain3.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain20.
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
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column4_row5) - (pedersen__points__y))) -
            ((column4_row7) * ((column4_row1) - (pedersen__points__x)));
        inner_sum += random_coefficients[63] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column4_row7) * (column4_row7)) -
            ((pedersen__hash0__ec_subset_sum__bit_0) *
             (((column4_row1) + (pedersen__points__x)) + (column4_row9)));
        inner_sum += random_coefficients[64] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column4_row5) + (column4_row13))) -
            ((column4_row7) * ((column4_row1) - (column4_row9)));
        inner_sum += random_coefficients[65] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column4_row9) - (column4_row1));
        inner_sum += random_coefficients[66] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column4_row13) - (column4_row5));
        inner_sum += random_coefficients[67] * constraint;
      }
      outer_sum += inner_sum * domain20;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain3);
  }

  {
    // Compute a sum of constraints with denominator = domain21.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column4_row3;
        inner_sum += random_coefficients[61] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain21);
  }

  {
    // Compute a sum of constraints with denominator = domain20.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column4_row3;
        inner_sum += random_coefficients[62] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain20);
  }

  {
    // Compute a sum of constraints with denominator = domain23.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/init/x:
        const FieldElementT constraint = (column4_row1) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[70] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/init/y:
        const FieldElementT constraint = (column4_row5) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[71] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value0:
        const FieldElementT constraint = (column1_row11) - (column4_row3);
        inner_sum += random_coefficients[72] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value0:
        const FieldElementT constraint = (column1_row2059) - (column4_row2051);
        inner_sum += random_coefficients[75] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_addr:
        const FieldElementT constraint =
            (column1_row2058) - ((column1_row10) + (FieldElementT::One()));
        inner_sum += random_coefficients[76] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value0:
        const FieldElementT constraint = (column1_row1035) - (column4_row4089);
        inner_sum += random_coefficients[77] * constraint;
      }
      {
        // Constraint expression for pedersen/output_addr:
        const FieldElementT constraint =
            (column1_row1034) - ((column1_row2058) + (FieldElementT::One()));
        inner_sum += random_coefficients[78] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain28.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input0_addr:
        const FieldElementT constraint =
            (column1_row4106) - ((column1_row1034) + (FieldElementT::One()));
        inner_sum += random_coefficients[73] * constraint;
      }
      outer_sum += inner_sum * domain28;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain23);
  }

  {
    // Compute a sum of constraints with denominator = domain9.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for range_check_builtin/value:
        const FieldElementT constraint = (range_check_builtin__value7_0) - (column1_row139);
        inner_sum += random_coefficients[79] * constraint;
      }
      {
        // Constraint expression for bitwise/x_or_y_addr:
        const FieldElementT constraint =
            (column1_row74) - ((column1_row234) + (FieldElementT::One()));
        inner_sum += random_coefficients[84] * constraint;
      }
      {
        // Constraint expression for bitwise/or_is_and_plus_xor:
        const FieldElementT constraint = (column1_row75) - ((column1_row171) + (column1_row235));
        inner_sum += random_coefficients[87] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking192:
        const FieldElementT constraint =
            (((column3_row176) + (column3_row240)) * (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
            (column3_row2);
        inner_sum += random_coefficients[89] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking193:
        const FieldElementT constraint =
            (((column3_row180) + (column3_row244)) * (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
            (column3_row130);
        inner_sum += random_coefficients[90] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking194:
        const FieldElementT constraint =
            (((column3_row184) + (column3_row248)) * (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
            (column3_row66);
        inner_sum += random_coefficients[91] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking195:
        const FieldElementT constraint = (((column3_row188) + (column3_row252)) *
                                          (FieldElementT::ConstexprFromBigInt(0x100_Z))) -
                                         (column3_row194);
        inner_sum += random_coefficients[92] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain29.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for range_check_builtin/addr_step:
        const FieldElementT constraint =
            (column1_row394) - ((column1_row138) + (FieldElementT::One()));
        inner_sum += random_coefficients[80] * constraint;
      }
      {
        // Constraint expression for bitwise/next_var_pool_addr:
        const FieldElementT constraint =
            (column1_row298) - ((column1_row74) + (FieldElementT::One()));
        inner_sum += random_coefficients[85] * constraint;
      }
      outer_sum += inner_sum * domain29;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain9);
  }

  {
    // Compute a sum of constraints with denominator = domain7.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain10.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/step_var_pool_addr:
        const FieldElementT constraint =
            (column1_row106) - ((column1_row42) + (FieldElementT::One()));
        inner_sum += random_coefficients[83] * constraint;
      }
      outer_sum += inner_sum * domain10;
    }

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/partition:
        const FieldElementT constraint =
            ((bitwise__sum_var_0_0) + (bitwise__sum_var_8_0)) - (column1_row43);
        inner_sum += random_coefficients[86] * constraint;
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
        // Constraint expression for bitwise/addition_is_xor_with_and:
        const FieldElementT constraint = ((column3_row0) + (column3_row64)) -
                                         (((column3_row192) + (column3_row128)) + (column3_row128));
        inner_sum += random_coefficients[88] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain11);
  }

  {
    // Compute a sum of constraints with denominator = domain12.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain30.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/param_0/addr_input_output_step:
        const FieldElementT constraint =
            (column1_row778) - ((column1_row266) + (FieldElementT::ConstexprFromBigInt(0x3_Z)));
        inner_sum += random_coefficients[94] * constraint;
      }
      {
        // Constraint expression for poseidon/param_1/addr_input_output_step:
        const FieldElementT constraint =
            (column1_row714) - ((column1_row202) + (FieldElementT::ConstexprFromBigInt(0x3_Z)));
        inner_sum += random_coefficients[96] * constraint;
      }
      {
        // Constraint expression for poseidon/param_2/addr_input_output_step:
        const FieldElementT constraint =
            (column1_row970) - ((column1_row458) + (FieldElementT::ConstexprFromBigInt(0x3_Z)));
        inner_sum += random_coefficients[98] * constraint;
      }
      outer_sum += inner_sum * domain30;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain12);
  }

  {
    // Compute a sum of constraints with denominator = domain8.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/full_rounds_state0_squaring:
        const FieldElementT constraint = ((column5_row9) * (column5_row9)) - (column5_row105);
        inner_sum += random_coefficients[99] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_rounds_state1_squaring:
        const FieldElementT constraint = ((column5_row73) * (column5_row73)) - (column5_row25);
        inner_sum += random_coefficients[100] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_rounds_state2_squaring:
        const FieldElementT constraint = ((column5_row41) * (column5_row41)) - (column5_row89);
        inner_sum += random_coefficients[101] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain13.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/full_round0:
        const FieldElementT constraint =
            (column5_row137) - ((((((poseidon__poseidon__full_rounds_state0_cubed_0) +
                                    (poseidon__poseidon__full_rounds_state0_cubed_0)) +
                                   (poseidon__poseidon__full_rounds_state0_cubed_0)) +
                                  (poseidon__poseidon__full_rounds_state1_cubed_0)) +
                                 (poseidon__poseidon__full_rounds_state2_cubed_0)) +
                                (poseidon__poseidon__full_round_key0));
        inner_sum += random_coefficients[107] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_round1:
        const FieldElementT constraint =
            ((column5_row201) + (poseidon__poseidon__full_rounds_state1_cubed_0)) -
            (((poseidon__poseidon__full_rounds_state0_cubed_0) +
              (poseidon__poseidon__full_rounds_state2_cubed_0)) +
             (poseidon__poseidon__full_round_key1));
        inner_sum += random_coefficients[108] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_round2:
        const FieldElementT constraint =
            (((column5_row169) + (poseidon__poseidon__full_rounds_state2_cubed_0)) +
             (poseidon__poseidon__full_rounds_state2_cubed_0)) -
            (((poseidon__poseidon__full_rounds_state0_cubed_0) +
              (poseidon__poseidon__full_rounds_state1_cubed_0)) +
             (poseidon__poseidon__full_round_key2));
        inner_sum += random_coefficients[109] * constraint;
      }
      outer_sum += inner_sum * domain13;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain8);
  }

  {
    // Compute a sum of constraints with denominator = domain6.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain16.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/partial_rounds_state1_squaring:
        const FieldElementT constraint = ((column5_row1) * (column5_row1)) - (column5_row17);
        inner_sum += random_coefficients[103] * constraint;
      }
      outer_sum += inner_sum * domain16;
    }

    {
      // Compute a sum of constraints with numerator = domain17.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/partial_round1:
        const FieldElementT constraint =
            (column5_row97) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__partial_rounds_state1_cubed_0)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column5_row33))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state1_cubed_1))) +
                (column5_row65)) +
               (column5_row65)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state1_cubed_2))) +
             (poseidon__poseidon__partial_round_key1));
        inner_sum += random_coefficients[120] * constraint;
      }
      outer_sum += inner_sum * domain17;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain6);
  }

  {
    // Compute a sum of constraints with denominator = domain18.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/add_first_round_key0:
        const FieldElementT constraint =
            ((column1_row267) +
             (FieldElementT::ConstexprFromBigInt(
                 0x6861759ea556a2339dd92f9562a30b9e58e2ad98109ae4780b7fd8eac77fe6f_Z))) -
            (column5_row9);
        inner_sum += random_coefficients[104] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/add_first_round_key1:
        const FieldElementT constraint =
            ((column1_row203) +
             (FieldElementT::ConstexprFromBigInt(
                 0x3827681995d5af9ffc8397a3d00425a3da43f76abf28a64e4ab1a22f27508c4_Z))) -
            (column5_row73);
        inner_sum += random_coefficients[105] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/add_first_round_key2:
        const FieldElementT constraint =
            ((column1_row459) +
             (FieldElementT::ConstexprFromBigInt(
                 0x3a3956d2fad44d0e7f760a2277dc7cb2cac75dc279b2d687a0dbe17704a8309_Z))) -
            (column5_row41);
        inner_sum += random_coefficients[106] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/last_full_round0:
        const FieldElementT constraint =
            (column1_row779) - (((((poseidon__poseidon__full_rounds_state0_cubed_7) +
                                   (poseidon__poseidon__full_rounds_state0_cubed_7)) +
                                  (poseidon__poseidon__full_rounds_state0_cubed_7)) +
                                 (poseidon__poseidon__full_rounds_state1_cubed_7)) +
                                (poseidon__poseidon__full_rounds_state2_cubed_7));
        inner_sum += random_coefficients[110] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/last_full_round1:
        const FieldElementT constraint =
            ((column1_row715) + (poseidon__poseidon__full_rounds_state1_cubed_7)) -
            ((poseidon__poseidon__full_rounds_state0_cubed_7) +
             (poseidon__poseidon__full_rounds_state2_cubed_7));
        inner_sum += random_coefficients[111] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/last_full_round2:
        const FieldElementT constraint =
            (((column1_row971) + (poseidon__poseidon__full_rounds_state2_cubed_7)) +
             (poseidon__poseidon__full_rounds_state2_cubed_7)) -
            ((poseidon__poseidon__full_rounds_state0_cubed_7) +
             (poseidon__poseidon__full_rounds_state1_cubed_7));
        inner_sum += random_coefficients[112] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/copy_partial_rounds0_i0:
        const FieldElementT constraint = (column5_row982) - (column5_row1);
        inner_sum += random_coefficients[113] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/copy_partial_rounds0_i1:
        const FieldElementT constraint = (column5_row998) - (column5_row33);
        inner_sum += random_coefficients[114] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/copy_partial_rounds0_i2:
        const FieldElementT constraint = (column5_row1014) - (column5_row65);
        inner_sum += random_coefficients[115] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_full_to_partial0:
        const FieldElementT constraint =
            (((column5_row6) + (poseidon__poseidon__full_rounds_state2_cubed_3)) +
             (poseidon__poseidon__full_rounds_state2_cubed_3)) -
            (((poseidon__poseidon__full_rounds_state0_cubed_3) +
              (poseidon__poseidon__full_rounds_state1_cubed_3)) +
             (FieldElementT::ConstexprFromBigInt(
                 0x4b085eb1df4258c3453cc97445954bf3433b6ab9dd5a99592864c00f54a3f9a_Z)));
        inner_sum += random_coefficients[116] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_full_to_partial1:
        const FieldElementT constraint =
            (column5_row22) -
            ((((((FieldElementT::ConstexprFromBigInt(
                     0x800000000000010fffffffffffffffffffffffffffffffffffffffffffffffd_Z)) *
                 (poseidon__poseidon__full_rounds_state1_cubed_3)) +
                ((FieldElementT::ConstexprFromBigInt(0xa_Z)) *
                 (poseidon__poseidon__full_rounds_state2_cubed_3))) +
               ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column5_row6))) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state0_cubed_0))) +
             (FieldElementT::ConstexprFromBigInt(
                 0x46fb825257fec76c50fe043684d4e6d2d2f2fdfe9b7c8d7128ca7acc0f66f30_Z)));
        inner_sum += random_coefficients[117] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_full_to_partial2:
        const FieldElementT constraint =
            (column5_row38) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__full_rounds_state2_cubed_3)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column5_row6))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state0_cubed_0))) +
                (column5_row22)) +
               (column5_row22)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state0_cubed_1))) +
             (FieldElementT::ConstexprFromBigInt(
                 0xf2193ba0c7ea33ce6222d9446c1e166202ae5461005292f4a2bcb93420151a_Z)));
        inner_sum += random_coefficients[118] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_partial_to_full0:
        const FieldElementT constraint =
            (column5_row521) -
            (((((((FieldElementT::ConstexprFromBigInt(0x10_Z)) *
                  (poseidon__poseidon__partial_rounds_state1_cubed_19)) +
                 ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column5_row641))) +
                ((FieldElementT::ConstexprFromBigInt(0x10_Z)) *
                 (poseidon__poseidon__partial_rounds_state1_cubed_20))) +
               ((FieldElementT::ConstexprFromBigInt(0x6_Z)) * (column5_row673))) +
              (poseidon__poseidon__partial_rounds_state1_cubed_21)) +
             (FieldElementT::ConstexprFromBigInt(
                 0x13d1b5cfd87693224f0ac561ab2c15ca53365d768311af59cefaf701bc53b37_Z)));
        inner_sum += random_coefficients[121] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_partial_to_full1:
        const FieldElementT constraint =
            (column5_row585) -
            ((((((FieldElementT::ConstexprFromBigInt(0x4_Z)) *
                 (poseidon__poseidon__partial_rounds_state1_cubed_20)) +
                (column5_row673)) +
               (column5_row673)) +
              (poseidon__poseidon__partial_rounds_state1_cubed_21)) +
             (FieldElementT::ConstexprFromBigInt(
                 0x3195d6b2d930e71cede286d5b8b41d49296ddf222bcd3bf3717a12a9a6947ff_Z)));
        inner_sum += random_coefficients[122] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_partial_to_full2:
        const FieldElementT constraint =
            (column5_row553) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__partial_rounds_state1_cubed_19)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column5_row641))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state1_cubed_20))) +
                (column5_row673)) +
               (column5_row673)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state1_cubed_21))) +
             (FieldElementT::ConstexprFromBigInt(
                 0x2c14fccabc26929170cc7ac9989c823608b9008bef3b8e16b6089a5d33cd72e_Z)));
        inner_sum += random_coefficients[123] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain18);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> CpuAirDefinition<FieldElementT, 12>::DomainEvalsAtPoint(
    gsl::span<const FieldElementT> point_powers,
    [[maybe_unused]] gsl::span<const FieldElementT> shifts) const {
  [[maybe_unused]] const FieldElementT& point = point_powers[0];
  const FieldElementT& domain0 = (point_powers[1]) - (FieldElementT::One());
  const FieldElementT& domain1 = (point_powers[2]) - (FieldElementT::One());
  const FieldElementT& domain2 = (point_powers[3]) - (FieldElementT::One());
  const FieldElementT& domain3 = (point_powers[4]) - (FieldElementT::One());
  const FieldElementT& domain4 = (point_powers[5]) - (shifts[0]);
  const FieldElementT& domain5 = (point_powers[5]) - (FieldElementT::One());
  const FieldElementT& domain6 = (point_powers[6]) - (FieldElementT::One());
  const FieldElementT& domain7 = (point_powers[7]) - (FieldElementT::One());
  const FieldElementT& domain8 = (point_powers[8]) - (FieldElementT::One());
  const FieldElementT& domain9 = (point_powers[9]) - (FieldElementT::One());
  const FieldElementT& domain10 = (point_powers[9]) - (shifts[1]);
  const FieldElementT& domain11 =
      ((point_powers[9]) - (shifts[2])) * ((point_powers[9]) - (shifts[3])) *
      ((point_powers[9]) - (shifts[4])) * ((point_powers[9]) - (shifts[5])) *
      ((point_powers[9]) - (shifts[6])) * ((point_powers[9]) - (shifts[7])) *
      ((point_powers[9]) - (shifts[8])) * ((point_powers[9]) - (shifts[9])) *
      ((point_powers[9]) - (shifts[10])) * ((point_powers[9]) - (shifts[11])) *
      ((point_powers[9]) - (shifts[12])) * ((point_powers[9]) - (shifts[13])) *
      ((point_powers[9]) - (shifts[14])) * ((point_powers[9]) - (shifts[15])) *
      ((point_powers[9]) - (shifts[16])) * (domain9);
  const FieldElementT& domain12 = (point_powers[10]) - (FieldElementT::One());
  const FieldElementT& domain13 = (point_powers[10]) - (shifts[1]);
  const FieldElementT& domain14 = (point_powers[11]) - (shifts[17]);
  const FieldElementT& domain15 =
      ((point_powers[11]) - (shifts[18])) * ((point_powers[11]) - (shifts[19])) * (domain14);
  const FieldElementT& domain16 =
      ((point_powers[11]) - (shifts[20])) * ((point_powers[11]) - (shifts[21])) *
      ((point_powers[11]) - (shifts[1])) * ((point_powers[11]) - (shifts[22])) *
      ((point_powers[11]) - (shifts[23])) * ((point_powers[11]) - (shifts[24])) *
      ((point_powers[11]) - (shifts[25])) * ((point_powers[11]) - (shifts[26])) *
      ((point_powers[11]) - (shifts[0])) * (domain14);
  const FieldElementT& domain17 = ((point_powers[11]) - (shifts[27])) *
                                  ((point_powers[11]) - (shifts[28])) *
                                  ((point_powers[11]) - (shifts[29])) * (domain16);
  const FieldElementT& domain18 = (point_powers[11]) - (FieldElementT::One());
  const FieldElementT& domain19 = (point_powers[12]) - (FieldElementT::One());
  const FieldElementT& domain20 = (point_powers[12]) - (shifts[30]);
  const FieldElementT& domain21 = (point_powers[12]) - (shifts[19]);
  const FieldElementT& domain22 = (point_powers[13]) - (shifts[31]);
  const FieldElementT& domain23 = (point_powers[13]) - (FieldElementT::One());
  return {
      domain0,  domain1,  domain2,  domain3,  domain4,  domain5,  domain6,  domain7,
      domain8,  domain9,  domain10, domain11, domain12, domain13, domain14, domain15,
      domain16, domain17, domain18, domain19, domain20, domain21, domain22, domain23,
  };
}

template <typename FieldElementT>
std::vector<uint64_t> CpuAirDefinition<FieldElementT, 12>::ParseDynamicParams(
    [[maybe_unused]] const std::map<std::string, uint64_t>& params) const {
  std::vector<uint64_t> result;

  ASSERT_RELEASE(params.size() == kNumDynamicParams, "Inconsistent dynamic data.");
  result.reserve(kNumDynamicParams);
  return result;
}

template <typename FieldElementT>
TraceGenerationContext CpuAirDefinition<FieldElementT, 12>::GetTraceGenerationContext() const {
  TraceGenerationContext ctx;

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 1024)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 512)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 512)) - (1)) >= (0), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 512)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(
      IsPowerOfTwo(trace_length_),
      "Coset step (MemberExpression(trace_length)) must be a power of two");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 256)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 256)) - (1)) >= (0), "step must not exceed dimension.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 256)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 4096)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 4096)) - (1)) >= (0), "step must not exceed dimension.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 4096)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 2)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 2)) - (1)) >= (0), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 2)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 4)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 4)) - (1)) >= (0), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 4)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 16)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 16)) - (1)) >= (0), "step must not exceed dimension.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 16)) >= (0), "Index out of range.");

  ASSERT_RELEASE(((trace_length_) - (1)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (2)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (3)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (6)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (4)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (8)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (9)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (5)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (13)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (11)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (7)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (15)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (18)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (10)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (74)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (42)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (106)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (26)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (90)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (2048)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (58)) >= (0), "Offset must be smaller than trace length.");

  ctx.AddVirtualColumn(
      "mem_pool/addr", VirtualColumn(/*column=*/kColumn1Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "mem_pool/value", VirtualColumn(/*column=*/kColumn1Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "range_check16_pool", VirtualColumn(/*column=*/kColumn4Column, /*step=*/4, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/mem_inst/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/mem_inst/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/pc", VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/instruction",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "cpu/decode/opcode_range_check/column",
      VirtualColumn(/*column=*/kColumn0Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/off0", VirtualColumn(/*column=*/kColumn4Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/off1", VirtualColumn(/*column=*/kColumn4Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/decode/off2", VirtualColumn(/*column=*/kColumn4Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/registers/ap", VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/registers/fp", VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_dst/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_dst/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op0/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op0/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op1/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "cpu/operands/mem_op1/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/13));
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
      "memory/sorted/addr", VirtualColumn(/*column=*/kColumn2Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/sorted/value",
      VirtualColumn(/*column=*/kColumn2Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "memory/multi_column_perm/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn6Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "orig/public_memory/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "orig/public_memory/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/16, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "range_check16/sorted",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/4, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "range_check16/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn7Inter1Column - kNumColumnsFirst, /*step=*/4, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "diluted_pool", VirtualColumn(/*column=*/kColumn3Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "diluted_check/permuted_values",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "diluted_check/cumulative_value",
      VirtualColumn(
          /*column=*/kColumn6Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "diluted_check/permutation/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn7Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/input0/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/4096, /*row_offset=*/10));
  ctx.AddVirtualColumn(
      "pedersen/input0/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/4096, /*row_offset=*/11));
  ctx.AddVirtualColumn(
      "pedersen/input1/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/4096, /*row_offset=*/2058));
  ctx.AddVirtualColumn(
      "pedersen/input1/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/4096, /*row_offset=*/2059));
  ctx.AddVirtualColumn(
      "pedersen/output/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/4096, /*row_offset=*/1034));
  ctx.AddVirtualColumn(
      "pedersen/output/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/4096, /*row_offset=*/1035));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/8, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/8, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/8, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/8, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/2048, /*row_offset=*/2047));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/2048, /*row_offset=*/57));
  ctx.AddVirtualColumn(
      "range_check_builtin/mem/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/138));
  ctx.AddVirtualColumn(
      "range_check_builtin/mem/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/139));
  ctx.AddVirtualColumn(
      "range_check_builtin/inner_range_check",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/32, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "bitwise/var_pool/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/64, /*row_offset=*/42));
  ctx.AddVirtualColumn(
      "bitwise/var_pool/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/64, /*row_offset=*/43));
  ctx.AddVirtualColumn(
      "bitwise/x/addr", VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/42));
  ctx.AddVirtualColumn(
      "bitwise/x/value", VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/43));
  ctx.AddVirtualColumn(
      "bitwise/y/addr", VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/106));
  ctx.AddVirtualColumn(
      "bitwise/y/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/107));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/170));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/171));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/234));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/235));
  ctx.AddVirtualColumn(
      "bitwise/x_or_y/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/74));
  ctx.AddVirtualColumn(
      "bitwise/x_or_y/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/256, /*row_offset=*/75));
  ctx.AddVirtualColumn(
      "bitwise/diluted_var_pool",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/4, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "bitwise/x", VirtualColumn(/*column=*/kColumn3Column, /*step=*/4, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "bitwise/y", VirtualColumn(/*column=*/kColumn3Column, /*step=*/4, /*row_offset=*/64));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y", VirtualColumn(/*column=*/kColumn3Column, /*step=*/4, /*row_offset=*/128));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y", VirtualColumn(/*column=*/kColumn3Column, /*step=*/4, /*row_offset=*/192));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking192",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/256, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking193",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/256, /*row_offset=*/130));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking194",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/256, /*row_offset=*/66));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking195",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/256, /*row_offset=*/194));
  ctx.AddVirtualColumn(
      "poseidon/param_0/input_output/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/512, /*row_offset=*/266));
  ctx.AddVirtualColumn(
      "poseidon/param_0/input_output/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/512, /*row_offset=*/267));
  ctx.AddVirtualColumn(
      "poseidon/param_1/input_output/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/512, /*row_offset=*/202));
  ctx.AddVirtualColumn(
      "poseidon/param_1/input_output/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/512, /*row_offset=*/203));
  ctx.AddVirtualColumn(
      "poseidon/param_2/input_output/addr",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/512, /*row_offset=*/458));
  ctx.AddVirtualColumn(
      "poseidon/param_2/input_output/value",
      VirtualColumn(/*column=*/kColumn1Column, /*step=*/512, /*row_offset=*/459));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state0",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/128, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state1",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/128, /*row_offset=*/73));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state2",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/128, /*row_offset=*/41));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state0_squared",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/128, /*row_offset=*/105));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state1_squared",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/128, /*row_offset=*/25));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state2_squared",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/128, /*row_offset=*/89));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state0",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state1",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/32, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state0_squared",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16, /*row_offset=*/14));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state1_squared",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/32, /*row_offset=*/17));

  ctx.AddPeriodicColumn(
      "pedersen/points/x",
      VirtualColumn(/*column=*/kPedersenPointsXPeriodicColumn, /*step=*/8, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "pedersen/points/y",
      VirtualColumn(/*column=*/kPedersenPointsYPeriodicColumn, /*step=*/8, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/full_round_key0",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonFullRoundKey0PeriodicColumn, /*step=*/128, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/full_round_key1",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonFullRoundKey1PeriodicColumn, /*step=*/128, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/full_round_key2",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonFullRoundKey2PeriodicColumn, /*step=*/128, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/partial_round_key0",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonPartialRoundKey0PeriodicColumn, /*step=*/16,
          /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/partial_round_key1",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonPartialRoundKey1PeriodicColumn, /*step=*/32,
          /*row_offset=*/0));

  ctx.AddObject<std::vector<size_t>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);

  return ctx;
}

template <typename FieldElementT>
std::vector<std::pair<int64_t, uint64_t>> CpuAirDefinition<FieldElementT, 12>::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;

  mask.reserve(192);
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
  mask.emplace_back(3, kColumn1Column);
  mask.emplace_back(4, kColumn1Column);
  mask.emplace_back(5, kColumn1Column);
  mask.emplace_back(8, kColumn1Column);
  mask.emplace_back(9, kColumn1Column);
  mask.emplace_back(10, kColumn1Column);
  mask.emplace_back(11, kColumn1Column);
  mask.emplace_back(12, kColumn1Column);
  mask.emplace_back(13, kColumn1Column);
  mask.emplace_back(16, kColumn1Column);
  mask.emplace_back(42, kColumn1Column);
  mask.emplace_back(43, kColumn1Column);
  mask.emplace_back(74, kColumn1Column);
  mask.emplace_back(75, kColumn1Column);
  mask.emplace_back(106, kColumn1Column);
  mask.emplace_back(138, kColumn1Column);
  mask.emplace_back(139, kColumn1Column);
  mask.emplace_back(171, kColumn1Column);
  mask.emplace_back(202, kColumn1Column);
  mask.emplace_back(203, kColumn1Column);
  mask.emplace_back(234, kColumn1Column);
  mask.emplace_back(235, kColumn1Column);
  mask.emplace_back(266, kColumn1Column);
  mask.emplace_back(267, kColumn1Column);
  mask.emplace_back(298, kColumn1Column);
  mask.emplace_back(394, kColumn1Column);
  mask.emplace_back(458, kColumn1Column);
  mask.emplace_back(459, kColumn1Column);
  mask.emplace_back(714, kColumn1Column);
  mask.emplace_back(715, kColumn1Column);
  mask.emplace_back(778, kColumn1Column);
  mask.emplace_back(779, kColumn1Column);
  mask.emplace_back(970, kColumn1Column);
  mask.emplace_back(971, kColumn1Column);
  mask.emplace_back(1034, kColumn1Column);
  mask.emplace_back(1035, kColumn1Column);
  mask.emplace_back(2058, kColumn1Column);
  mask.emplace_back(2059, kColumn1Column);
  mask.emplace_back(4106, kColumn1Column);
  mask.emplace_back(0, kColumn2Column);
  mask.emplace_back(1, kColumn2Column);
  mask.emplace_back(2, kColumn2Column);
  mask.emplace_back(3, kColumn2Column);
  mask.emplace_back(0, kColumn3Column);
  mask.emplace_back(1, kColumn3Column);
  mask.emplace_back(2, kColumn3Column);
  mask.emplace_back(3, kColumn3Column);
  mask.emplace_back(4, kColumn3Column);
  mask.emplace_back(8, kColumn3Column);
  mask.emplace_back(12, kColumn3Column);
  mask.emplace_back(16, kColumn3Column);
  mask.emplace_back(20, kColumn3Column);
  mask.emplace_back(24, kColumn3Column);
  mask.emplace_back(28, kColumn3Column);
  mask.emplace_back(32, kColumn3Column);
  mask.emplace_back(36, kColumn3Column);
  mask.emplace_back(40, kColumn3Column);
  mask.emplace_back(44, kColumn3Column);
  mask.emplace_back(48, kColumn3Column);
  mask.emplace_back(52, kColumn3Column);
  mask.emplace_back(56, kColumn3Column);
  mask.emplace_back(60, kColumn3Column);
  mask.emplace_back(64, kColumn3Column);
  mask.emplace_back(66, kColumn3Column);
  mask.emplace_back(128, kColumn3Column);
  mask.emplace_back(130, kColumn3Column);
  mask.emplace_back(176, kColumn3Column);
  mask.emplace_back(180, kColumn3Column);
  mask.emplace_back(184, kColumn3Column);
  mask.emplace_back(188, kColumn3Column);
  mask.emplace_back(192, kColumn3Column);
  mask.emplace_back(194, kColumn3Column);
  mask.emplace_back(240, kColumn3Column);
  mask.emplace_back(244, kColumn3Column);
  mask.emplace_back(248, kColumn3Column);
  mask.emplace_back(252, kColumn3Column);
  mask.emplace_back(0, kColumn4Column);
  mask.emplace_back(1, kColumn4Column);
  mask.emplace_back(2, kColumn4Column);
  mask.emplace_back(3, kColumn4Column);
  mask.emplace_back(4, kColumn4Column);
  mask.emplace_back(5, kColumn4Column);
  mask.emplace_back(6, kColumn4Column);
  mask.emplace_back(7, kColumn4Column);
  mask.emplace_back(8, kColumn4Column);
  mask.emplace_back(9, kColumn4Column);
  mask.emplace_back(11, kColumn4Column);
  mask.emplace_back(12, kColumn4Column);
  mask.emplace_back(13, kColumn4Column);
  mask.emplace_back(44, kColumn4Column);
  mask.emplace_back(76, kColumn4Column);
  mask.emplace_back(108, kColumn4Column);
  mask.emplace_back(140, kColumn4Column);
  mask.emplace_back(172, kColumn4Column);
  mask.emplace_back(204, kColumn4Column);
  mask.emplace_back(236, kColumn4Column);
  mask.emplace_back(1539, kColumn4Column);
  mask.emplace_back(1547, kColumn4Column);
  mask.emplace_back(1571, kColumn4Column);
  mask.emplace_back(1579, kColumn4Column);
  mask.emplace_back(2011, kColumn4Column);
  mask.emplace_back(2019, kColumn4Column);
  mask.emplace_back(2041, kColumn4Column);
  mask.emplace_back(2045, kColumn4Column);
  mask.emplace_back(2047, kColumn4Column);
  mask.emplace_back(2049, kColumn4Column);
  mask.emplace_back(2051, kColumn4Column);
  mask.emplace_back(2053, kColumn4Column);
  mask.emplace_back(4089, kColumn4Column);
  mask.emplace_back(0, kColumn5Column);
  mask.emplace_back(1, kColumn5Column);
  mask.emplace_back(2, kColumn5Column);
  mask.emplace_back(4, kColumn5Column);
  mask.emplace_back(6, kColumn5Column);
  mask.emplace_back(8, kColumn5Column);
  mask.emplace_back(9, kColumn5Column);
  mask.emplace_back(10, kColumn5Column);
  mask.emplace_back(12, kColumn5Column);
  mask.emplace_back(14, kColumn5Column);
  mask.emplace_back(16, kColumn5Column);
  mask.emplace_back(17, kColumn5Column);
  mask.emplace_back(22, kColumn5Column);
  mask.emplace_back(24, kColumn5Column);
  mask.emplace_back(25, kColumn5Column);
  mask.emplace_back(30, kColumn5Column);
  mask.emplace_back(33, kColumn5Column);
  mask.emplace_back(38, kColumn5Column);
  mask.emplace_back(41, kColumn5Column);
  mask.emplace_back(46, kColumn5Column);
  mask.emplace_back(49, kColumn5Column);
  mask.emplace_back(54, kColumn5Column);
  mask.emplace_back(57, kColumn5Column);
  mask.emplace_back(65, kColumn5Column);
  mask.emplace_back(73, kColumn5Column);
  mask.emplace_back(81, kColumn5Column);
  mask.emplace_back(89, kColumn5Column);
  mask.emplace_back(97, kColumn5Column);
  mask.emplace_back(105, kColumn5Column);
  mask.emplace_back(137, kColumn5Column);
  mask.emplace_back(169, kColumn5Column);
  mask.emplace_back(201, kColumn5Column);
  mask.emplace_back(393, kColumn5Column);
  mask.emplace_back(409, kColumn5Column);
  mask.emplace_back(425, kColumn5Column);
  mask.emplace_back(457, kColumn5Column);
  mask.emplace_back(473, kColumn5Column);
  mask.emplace_back(489, kColumn5Column);
  mask.emplace_back(521, kColumn5Column);
  mask.emplace_back(553, kColumn5Column);
  mask.emplace_back(585, kColumn5Column);
  mask.emplace_back(609, kColumn5Column);
  mask.emplace_back(625, kColumn5Column);
  mask.emplace_back(641, kColumn5Column);
  mask.emplace_back(657, kColumn5Column);
  mask.emplace_back(673, kColumn5Column);
  mask.emplace_back(689, kColumn5Column);
  mask.emplace_back(905, kColumn5Column);
  mask.emplace_back(921, kColumn5Column);
  mask.emplace_back(937, kColumn5Column);
  mask.emplace_back(969, kColumn5Column);
  mask.emplace_back(982, kColumn5Column);
  mask.emplace_back(985, kColumn5Column);
  mask.emplace_back(998, kColumn5Column);
  mask.emplace_back(1001, kColumn5Column);
  mask.emplace_back(1014, kColumn5Column);
  mask.emplace_back(0, kColumn6Inter1Column);
  mask.emplace_back(1, kColumn6Inter1Column);
  mask.emplace_back(2, kColumn6Inter1Column);
  mask.emplace_back(3, kColumn6Inter1Column);
  mask.emplace_back(0, kColumn7Inter1Column);
  mask.emplace_back(1, kColumn7Inter1Column);
  mask.emplace_back(2, kColumn7Inter1Column);
  mask.emplace_back(5, kColumn7Inter1Column);

  return mask;
}

}  // namespace cpu

}  // namespace starkware
