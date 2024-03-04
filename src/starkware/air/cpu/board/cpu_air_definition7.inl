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
CpuAirDefinition<FieldElementT, 7>::CreateCompositionPolynomial(
    const FieldElement& trace_generator, const ConstFieldElementSpan& random_coefficients) const {
  Builder builder(kNumPeriodicColumns);
  const FieldElementT& gen = trace_generator.As<FieldElementT>();

  const std::vector<uint64_t> point_exponents = {
      uint64_t(trace_length_),
      uint64_t(SafeDiv(trace_length_, 2)),
      uint64_t(SafeDiv(trace_length_, 4)),
      uint64_t(SafeDiv(trace_length_, 16)),
      uint64_t(SafeDiv(trace_length_, 32)),
      uint64_t(SafeDiv(trace_length_, 64)),
      uint64_t(SafeDiv(trace_length_, 128)),
      uint64_t(SafeDiv(trace_length_, 1024)),
      uint64_t(SafeDiv(trace_length_, 2048))};
  const std::vector<uint64_t> gen_exponents = {
      uint64_t(SafeDiv((15) * (trace_length_), 16)),
      uint64_t(SafeDiv((3) * (trace_length_), 4)),
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
      uint64_t(SafeDiv((255) * (trace_length_), 256)),
      uint64_t(SafeDiv(trace_length_, 2)),
      uint64_t((trace_length_) - (1)),
      uint64_t((trace_length_) - (16)),
      uint64_t((trace_length_) - (2)),
      uint64_t((trace_length_) - (4)),
      uint64_t((trace_length_) - (2048)),
      uint64_t((trace_length_) - (128)),
      uint64_t((trace_length_) - (64))};

  BuildAutoPeriodicColumns(gen, &builder);

  BuildPeriodicColumns(gen, &builder);

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
void CpuAirDefinition<FieldElementT, 7>::BuildAutoPeriodicColumns(
    const FieldElementT& gen, Builder* builder) const {
  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonFullRoundKey0PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 16),
      kPoseidonPoseidonFullRoundKey0PeriodicColumn);

  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonFullRoundKey1PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 16),
      kPoseidonPoseidonFullRoundKey1PeriodicColumn);

  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonFullRoundKey2PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 16),
      kPoseidonPoseidonFullRoundKey2PeriodicColumn);

  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonPartialRoundKey0PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 2),
      kPoseidonPoseidonPartialRoundKey0PeriodicColumn);

  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonPartialRoundKey1PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 4),
      kPoseidonPoseidonPartialRoundKey1PeriodicColumn);
};

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
CpuAirDefinition<FieldElementT, 7>::PrecomputeDomainEvalsOnCoset(
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
      FieldElementT::UninitializedVector(4),    FieldElementT::UninitializedVector(16),
      FieldElementT::UninitializedVector(16),   FieldElementT::UninitializedVector(32),
      FieldElementT::UninitializedVector(64),   FieldElementT::UninitializedVector(64),
      FieldElementT::UninitializedVector(128),  FieldElementT::UninitializedVector(128),
      FieldElementT::UninitializedVector(128),  FieldElementT::UninitializedVector(128),
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

  period = 64;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[6][i] = (point_powers[5][i & (63)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 64;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[7][i] = (point_powers[5][i & (63)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[8][i] = (point_powers[6][i & (127)]) - (shifts[2]);
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[9][i] = ((point_powers[6][i & (127)]) - (shifts[3])) *
                                  ((point_powers[6][i & (127)]) - (shifts[4])) *
                                  (precomp_domains[8][i & (128 - 1)]);
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[10][i] = (point_powers[6][i & (127)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[11][i] = ((point_powers[6][i & (127)]) - (shifts[5])) *
                                   ((point_powers[6][i & (127)]) - (shifts[6])) *
                                   ((point_powers[6][i & (127)]) - (shifts[7])) *
                                   ((point_powers[6][i & (127)]) - (shifts[8])) *
                                   ((point_powers[6][i & (127)]) - (shifts[9])) *
                                   ((point_powers[6][i & (127)]) - (shifts[10])) *
                                   ((point_powers[6][i & (127)]) - (shifts[11])) *
                                   ((point_powers[6][i & (127)]) - (shifts[0])) *
                                   (precomp_domains[8][i & (128 - 1)]) *
                                   (precomp_domains[10][i & (128 - 1)]);
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[12][i] = ((point_powers[6][i & (127)]) - (shifts[12])) *
                                   ((point_powers[6][i & (127)]) - (shifts[13])) *
                                   ((point_powers[6][i & (127)]) - (shifts[14])) *
                                   (precomp_domains[11][i & (128 - 1)]);
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[13][i] = (point_powers[6][i & (127)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 128;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[14][i] = ((point_powers[6][i & (127)]) - (shifts[15])) *
                                   ((point_powers[6][i & (127)]) - (shifts[16])) *
                                   ((point_powers[6][i & (127)]) - (shifts[17])) *
                                   ((point_powers[6][i & (127)]) - (shifts[18])) *
                                   ((point_powers[6][i & (127)]) - (shifts[19])) *
                                   ((point_powers[6][i & (127)]) - (shifts[20])) *
                                   ((point_powers[6][i & (127)]) - (shifts[21])) *
                                   ((point_powers[6][i & (127)]) - (shifts[22])) *
                                   ((point_powers[6][i & (127)]) - (shifts[23])) *
                                   ((point_powers[6][i & (127)]) - (shifts[24])) *
                                   ((point_powers[6][i & (127)]) - (shifts[25])) *
                                   ((point_powers[6][i & (127)]) - (shifts[26])) *
                                   ((point_powers[6][i & (127)]) - (shifts[27])) *
                                   ((point_powers[6][i & (127)]) - (shifts[28])) *
                                   ((point_powers[6][i & (127)]) - (shifts[29])) *
                                   (precomp_domains[13][i & (128 - 1)]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[15][i] = (point_powers[7][i & (1023)]) - (shifts[30]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[16][i] = (point_powers[7][i & (1023)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[17][i] = (point_powers[7][i & (1023)]) - (shifts[4]);
        }
      },
      period, kTaskSize);

  period = 2048;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[18][i] = (point_powers[8][i & (2047)]) - (shifts[31]);
        }
      },
      period, kTaskSize);

  period = 2048;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[19][i] = (point_powers[8][i & (2047)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);
  return precomp_domains;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> CpuAirDefinition<FieldElementT, 7>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, [[maybe_unused]] const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 39, "shifts should contain 39 elements.");

  // domain0 = point^trace_length - 1.
  [[maybe_unused]] const FieldElementT& domain0 = precomp_domains[0];
  // domain1 = point^(trace_length / 2) - 1.
  [[maybe_unused]] const FieldElementT& domain1 = precomp_domains[1];
  // domain2 = point^(trace_length / 4) - 1.
  [[maybe_unused]] const FieldElementT& domain2 = precomp_domains[2];
  // domain3 = point^(trace_length / 16) - gen^(15 * trace_length / 16).
  [[maybe_unused]] const FieldElementT& domain3 = precomp_domains[3];
  // domain4 = point^(trace_length / 16) - 1.
  [[maybe_unused]] const FieldElementT& domain4 = precomp_domains[4];
  // domain5 = point^(trace_length / 32) - 1.
  [[maybe_unused]] const FieldElementT& domain5 = precomp_domains[5];
  // domain6 = point^(trace_length / 64) - gen^(3 * trace_length / 4).
  [[maybe_unused]] const FieldElementT& domain6 = precomp_domains[6];
  // domain7 = point^(trace_length / 64) - 1.
  [[maybe_unused]] const FieldElementT& domain7 = precomp_domains[7];
  // domain8 = point^(trace_length / 128) - gen^(31 * trace_length / 32).
  [[maybe_unused]] const FieldElementT& domain8 = precomp_domains[8];
  // domain9 = (point^(trace_length / 128) - gen^(61 * trace_length / 64)) * (point^(trace_length /
  // 128) - gen^(63 * trace_length / 64)) * domain8.
  [[maybe_unused]] const FieldElementT& domain9 = precomp_domains[9];
  // domain10 = point^(trace_length / 128) - gen^(3 * trace_length / 4).
  [[maybe_unused]] const FieldElementT& domain10 = precomp_domains[10];
  // domain11 = (point^(trace_length / 128) - gen^(11 * trace_length / 16)) * (point^(trace_length /
  // 128) - gen^(23 * trace_length / 32)) * (point^(trace_length / 128) - gen^(25 * trace_length /
  // 32)) * (point^(trace_length / 128) - gen^(13 * trace_length / 16)) * (point^(trace_length /
  // 128) - gen^(27 * trace_length / 32)) * (point^(trace_length / 128) - gen^(7 * trace_length /
  // 8)) * (point^(trace_length / 128) - gen^(29 * trace_length / 32)) * (point^(trace_length / 128)
  // - gen^(15 * trace_length / 16)) * domain8 * domain10.
  [[maybe_unused]] const FieldElementT& domain11 = precomp_domains[11];
  // domain12 = (point^(trace_length / 128) - gen^(19 * trace_length / 32)) * (point^(trace_length /
  // 128) - gen^(5 * trace_length / 8)) * (point^(trace_length / 128) - gen^(21 * trace_length /
  // 32)) * domain11.
  [[maybe_unused]] const FieldElementT& domain12 = precomp_domains[12];
  // domain13 = point^(trace_length / 128) - 1.
  [[maybe_unused]] const FieldElementT& domain13 = precomp_domains[13];
  // domain14 = (point^(trace_length / 128) - gen^(trace_length / 64)) * (point^(trace_length / 128)
  // - gen^(trace_length / 32)) * (point^(trace_length / 128) - gen^(3 * trace_length / 64)) *
  // (point^(trace_length / 128) - gen^(trace_length / 16)) * (point^(trace_length / 128) - gen^(5 *
  // trace_length / 64)) * (point^(trace_length / 128) - gen^(3 * trace_length / 32)) *
  // (point^(trace_length / 128) - gen^(7 * trace_length / 64)) * (point^(trace_length / 128) -
  // gen^(trace_length / 8)) * (point^(trace_length / 128) - gen^(9 * trace_length / 64)) *
  // (point^(trace_length / 128) - gen^(5 * trace_length / 32)) * (point^(trace_length / 128) -
  // gen^(11 * trace_length / 64)) * (point^(trace_length / 128) - gen^(3 * trace_length / 16)) *
  // (point^(trace_length / 128) - gen^(13 * trace_length / 64)) * (point^(trace_length / 128) -
  // gen^(7 * trace_length / 32)) * (point^(trace_length / 128) - gen^(15 * trace_length / 64)) *
  // domain13.
  [[maybe_unused]] const FieldElementT& domain14 = precomp_domains[14];
  // domain15 = point^(trace_length / 1024) - gen^(255 * trace_length / 256).
  [[maybe_unused]] const FieldElementT& domain15 = precomp_domains[15];
  // domain16 = point^(trace_length / 1024) - 1.
  [[maybe_unused]] const FieldElementT& domain16 = precomp_domains[16];
  // domain17 = point^(trace_length / 1024) - gen^(63 * trace_length / 64).
  [[maybe_unused]] const FieldElementT& domain17 = precomp_domains[17];
  // domain18 = point^(trace_length / 2048) - gen^(trace_length / 2).
  [[maybe_unused]] const FieldElementT& domain18 = precomp_domains[18];
  // domain19 = point^(trace_length / 2048) - 1.
  [[maybe_unused]] const FieldElementT& domain19 = precomp_domains[19];
  // domain20 = point - gen^(trace_length - 1).
  const FieldElementT& domain20 = (point) - (shifts[32]);
  // domain21 = point - gen^(trace_length - 16).
  const FieldElementT& domain21 = (point) - (shifts[33]);
  // domain22 = point - 1.
  const FieldElementT& domain22 = (point) - (FieldElementT::One());
  // domain23 = point - gen^(trace_length - 2).
  const FieldElementT& domain23 = (point) - (shifts[34]);
  // domain24 = point - gen^(trace_length - 4).
  const FieldElementT& domain24 = (point) - (shifts[35]);
  // domain25 = point - gen^(trace_length - 2048).
  const FieldElementT& domain25 = (point) - (shifts[36]);
  // domain26 = point - gen^(trace_length - 128).
  const FieldElementT& domain26 = (point) - (shifts[37]);
  // domain27 = point - gen^(trace_length - 64).
  const FieldElementT& domain27 = (point) - (shifts[38]);

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
  const FieldElementT& column3_row6 = neighbors[kColumn3Row6Neighbor];
  const FieldElementT& column3_row7 = neighbors[kColumn3Row7Neighbor];
  const FieldElementT& column3_row8 = neighbors[kColumn3Row8Neighbor];
  const FieldElementT& column3_row9 = neighbors[kColumn3Row9Neighbor];
  const FieldElementT& column3_row10 = neighbors[kColumn3Row10Neighbor];
  const FieldElementT& column3_row11 = neighbors[kColumn3Row11Neighbor];
  const FieldElementT& column3_row12 = neighbors[kColumn3Row12Neighbor];
  const FieldElementT& column3_row13 = neighbors[kColumn3Row13Neighbor];
  const FieldElementT& column3_row16 = neighbors[kColumn3Row16Neighbor];
  const FieldElementT& column3_row22 = neighbors[kColumn3Row22Neighbor];
  const FieldElementT& column3_row23 = neighbors[kColumn3Row23Neighbor];
  const FieldElementT& column3_row26 = neighbors[kColumn3Row26Neighbor];
  const FieldElementT& column3_row27 = neighbors[kColumn3Row27Neighbor];
  const FieldElementT& column3_row38 = neighbors[kColumn3Row38Neighbor];
  const FieldElementT& column3_row39 = neighbors[kColumn3Row39Neighbor];
  const FieldElementT& column3_row42 = neighbors[kColumn3Row42Neighbor];
  const FieldElementT& column3_row43 = neighbors[kColumn3Row43Neighbor];
  const FieldElementT& column3_row58 = neighbors[kColumn3Row58Neighbor];
  const FieldElementT& column3_row70 = neighbors[kColumn3Row70Neighbor];
  const FieldElementT& column3_row71 = neighbors[kColumn3Row71Neighbor];
  const FieldElementT& column3_row74 = neighbors[kColumn3Row74Neighbor];
  const FieldElementT& column3_row75 = neighbors[kColumn3Row75Neighbor];
  const FieldElementT& column3_row86 = neighbors[kColumn3Row86Neighbor];
  const FieldElementT& column3_row87 = neighbors[kColumn3Row87Neighbor];
  const FieldElementT& column3_row91 = neighbors[kColumn3Row91Neighbor];
  const FieldElementT& column3_row102 = neighbors[kColumn3Row102Neighbor];
  const FieldElementT& column3_row103 = neighbors[kColumn3Row103Neighbor];
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
  const FieldElementT& column5_row122 = neighbors[kColumn5Row122Neighbor];
  const FieldElementT& column5_row124 = neighbors[kColumn5Row124Neighbor];
  const FieldElementT& column5_row126 = neighbors[kColumn5Row126Neighbor];
  const FieldElementT& column6_row0 = neighbors[kColumn6Row0Neighbor];
  const FieldElementT& column6_row1 = neighbors[kColumn6Row1Neighbor];
  const FieldElementT& column6_row2 = neighbors[kColumn6Row2Neighbor];
  const FieldElementT& column6_row3 = neighbors[kColumn6Row3Neighbor];
  const FieldElementT& column6_row4 = neighbors[kColumn6Row4Neighbor];
  const FieldElementT& column6_row5 = neighbors[kColumn6Row5Neighbor];
  const FieldElementT& column6_row6 = neighbors[kColumn6Row6Neighbor];
  const FieldElementT& column6_row7 = neighbors[kColumn6Row7Neighbor];
  const FieldElementT& column6_row8 = neighbors[kColumn6Row8Neighbor];
  const FieldElementT& column6_row12 = neighbors[kColumn6Row12Neighbor];
  const FieldElementT& column6_row28 = neighbors[kColumn6Row28Neighbor];
  const FieldElementT& column6_row44 = neighbors[kColumn6Row44Neighbor];
  const FieldElementT& column6_row60 = neighbors[kColumn6Row60Neighbor];
  const FieldElementT& column6_row76 = neighbors[kColumn6Row76Neighbor];
  const FieldElementT& column6_row92 = neighbors[kColumn6Row92Neighbor];
  const FieldElementT& column6_row108 = neighbors[kColumn6Row108Neighbor];
  const FieldElementT& column6_row124 = neighbors[kColumn6Row124Neighbor];
  const FieldElementT& column6_row1021 = neighbors[kColumn6Row1021Neighbor];
  const FieldElementT& column6_row1023 = neighbors[kColumn6Row1023Neighbor];
  const FieldElementT& column6_row1025 = neighbors[kColumn6Row1025Neighbor];
  const FieldElementT& column6_row1027 = neighbors[kColumn6Row1027Neighbor];
  const FieldElementT& column6_row2045 = neighbors[kColumn6Row2045Neighbor];
  const FieldElementT& column7_row0 = neighbors[kColumn7Row0Neighbor];
  const FieldElementT& column7_row1 = neighbors[kColumn7Row1Neighbor];
  const FieldElementT& column7_row2 = neighbors[kColumn7Row2Neighbor];
  const FieldElementT& column7_row3 = neighbors[kColumn7Row3Neighbor];
  const FieldElementT& column7_row4 = neighbors[kColumn7Row4Neighbor];
  const FieldElementT& column7_row5 = neighbors[kColumn7Row5Neighbor];
  const FieldElementT& column7_row7 = neighbors[kColumn7Row7Neighbor];
  const FieldElementT& column7_row9 = neighbors[kColumn7Row9Neighbor];
  const FieldElementT& column7_row11 = neighbors[kColumn7Row11Neighbor];
  const FieldElementT& column7_row13 = neighbors[kColumn7Row13Neighbor];
  const FieldElementT& column7_row77 = neighbors[kColumn7Row77Neighbor];
  const FieldElementT& column7_row79 = neighbors[kColumn7Row79Neighbor];
  const FieldElementT& column7_row81 = neighbors[kColumn7Row81Neighbor];
  const FieldElementT& column7_row83 = neighbors[kColumn7Row83Neighbor];
  const FieldElementT& column7_row85 = neighbors[kColumn7Row85Neighbor];
  const FieldElementT& column7_row87 = neighbors[kColumn7Row87Neighbor];
  const FieldElementT& column7_row89 = neighbors[kColumn7Row89Neighbor];
  const FieldElementT& column7_row768 = neighbors[kColumn7Row768Neighbor];
  const FieldElementT& column7_row772 = neighbors[kColumn7Row772Neighbor];
  const FieldElementT& column7_row784 = neighbors[kColumn7Row784Neighbor];
  const FieldElementT& column7_row788 = neighbors[kColumn7Row788Neighbor];
  const FieldElementT& column7_row1004 = neighbors[kColumn7Row1004Neighbor];
  const FieldElementT& column7_row1008 = neighbors[kColumn7Row1008Neighbor];
  const FieldElementT& column7_row1022 = neighbors[kColumn7Row1022Neighbor];
  const FieldElementT& column7_row1024 = neighbors[kColumn7Row1024Neighbor];
  const FieldElementT& column8_row0 = neighbors[kColumn8Row0Neighbor];
  const FieldElementT& column8_row1 = neighbors[kColumn8Row1Neighbor];
  const FieldElementT& column8_row2 = neighbors[kColumn8Row2Neighbor];
  const FieldElementT& column8_row4 = neighbors[kColumn8Row4Neighbor];
  const FieldElementT& column8_row5 = neighbors[kColumn8Row5Neighbor];
  const FieldElementT& column8_row6 = neighbors[kColumn8Row6Neighbor];
  const FieldElementT& column8_row8 = neighbors[kColumn8Row8Neighbor];
  const FieldElementT& column8_row9 = neighbors[kColumn8Row9Neighbor];
  const FieldElementT& column8_row10 = neighbors[kColumn8Row10Neighbor];
  const FieldElementT& column8_row12 = neighbors[kColumn8Row12Neighbor];
  const FieldElementT& column8_row13 = neighbors[kColumn8Row13Neighbor];
  const FieldElementT& column8_row14 = neighbors[kColumn8Row14Neighbor];
  const FieldElementT& column8_row16 = neighbors[kColumn8Row16Neighbor];
  const FieldElementT& column8_row17 = neighbors[kColumn8Row17Neighbor];
  const FieldElementT& column8_row22 = neighbors[kColumn8Row22Neighbor];
  const FieldElementT& column8_row24 = neighbors[kColumn8Row24Neighbor];
  const FieldElementT& column8_row30 = neighbors[kColumn8Row30Neighbor];
  const FieldElementT& column8_row49 = neighbors[kColumn8Row49Neighbor];
  const FieldElementT& column8_row53 = neighbors[kColumn8Row53Neighbor];
  const FieldElementT& column8_row54 = neighbors[kColumn8Row54Neighbor];
  const FieldElementT& column8_row57 = neighbors[kColumn8Row57Neighbor];
  const FieldElementT& column8_row61 = neighbors[kColumn8Row61Neighbor];
  const FieldElementT& column8_row62 = neighbors[kColumn8Row62Neighbor];
  const FieldElementT& column8_row65 = neighbors[kColumn8Row65Neighbor];
  const FieldElementT& column8_row70 = neighbors[kColumn8Row70Neighbor];
  const FieldElementT& column8_row78 = neighbors[kColumn8Row78Neighbor];
  const FieldElementT& column8_row113 = neighbors[kColumn8Row113Neighbor];
  const FieldElementT& column8_row117 = neighbors[kColumn8Row117Neighbor];
  const FieldElementT& column8_row118 = neighbors[kColumn8Row118Neighbor];
  const FieldElementT& column8_row121 = neighbors[kColumn8Row121Neighbor];
  const FieldElementT& column8_row125 = neighbors[kColumn8Row125Neighbor];
  const FieldElementT& column8_row126 = neighbors[kColumn8Row126Neighbor];
  const FieldElementT& column9_inter1_row0 = neighbors[kColumn9Inter1Row0Neighbor];
  const FieldElementT& column9_inter1_row1 = neighbors[kColumn9Inter1Row1Neighbor];
  const FieldElementT& column10_inter1_row0 = neighbors[kColumn10Inter1Row0Neighbor];
  const FieldElementT& column10_inter1_row1 = neighbors[kColumn10Inter1Row1Neighbor];
  const FieldElementT& column11_inter1_row0 = neighbors[kColumn11Inter1Row0Neighbor];
  const FieldElementT& column11_inter1_row1 = neighbors[kColumn11Inter1Row1Neighbor];
  const FieldElementT& column11_inter1_row2 = neighbors[kColumn11Inter1Row2Neighbor];
  const FieldElementT& column11_inter1_row5 = neighbors[kColumn11Inter1Row5Neighbor];

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
      ((column3_row0) + (cpu__decode__opcode_range_check__bit_2)) + (FieldElementT::One());
  const FieldElementT cpu__decode__opcode_range_check__bit_10 =
      (column0_row10) - ((column0_row11) + (column0_row11));
  const FieldElementT cpu__decode__opcode_range_check__bit_11 =
      (column0_row11) - ((column0_row12) + (column0_row12));
  const FieldElementT cpu__decode__opcode_range_check__bit_14 =
      (column0_row14) - ((column0_row15) + (column0_row15));
  const FieldElementT memory__address_diff_0 = (column4_row2) - (column4_row0);
  const FieldElementT range_check16__diff_0 = (column6_row6) - (column6_row2);
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_0 =
      (column7_row0) - ((column7_row4) + (column7_row4));
  const FieldElementT pedersen__hash0__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (pedersen__hash0__ec_subset_sum__bit_0);
  const FieldElementT range_check_builtin__value0_0 = column6_row12;
  const FieldElementT range_check_builtin__value1_0 =
      ((range_check_builtin__value0_0) * (offset_size_)) + (column6_row28);
  const FieldElementT range_check_builtin__value2_0 =
      ((range_check_builtin__value1_0) * (offset_size_)) + (column6_row44);
  const FieldElementT range_check_builtin__value3_0 =
      ((range_check_builtin__value2_0) * (offset_size_)) + (column6_row60);
  const FieldElementT range_check_builtin__value4_0 =
      ((range_check_builtin__value3_0) * (offset_size_)) + (column6_row76);
  const FieldElementT range_check_builtin__value5_0 =
      ((range_check_builtin__value4_0) * (offset_size_)) + (column6_row92);
  const FieldElementT range_check_builtin__value6_0 =
      ((range_check_builtin__value5_0) * (offset_size_)) + (column6_row108);
  const FieldElementT range_check_builtin__value7_0 =
      ((range_check_builtin__value6_0) * (offset_size_)) + (column6_row124);
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
  const FieldElementT poseidon__poseidon__full_rounds_state0_cubed_0 =
      (column8_row6) * (column8_row9);
  const FieldElementT poseidon__poseidon__full_rounds_state1_cubed_0 =
      (column8_row14) * (column8_row5);
  const FieldElementT poseidon__poseidon__full_rounds_state2_cubed_0 =
      (column8_row1) * (column8_row13);
  const FieldElementT poseidon__poseidon__full_rounds_state0_cubed_7 =
      (column8_row118) * (column8_row121);
  const FieldElementT poseidon__poseidon__full_rounds_state1_cubed_7 =
      (column8_row126) * (column8_row117);
  const FieldElementT poseidon__poseidon__full_rounds_state2_cubed_7 =
      (column8_row113) * (column8_row125);
  const FieldElementT poseidon__poseidon__full_rounds_state0_cubed_3 =
      (column8_row54) * (column8_row57);
  const FieldElementT poseidon__poseidon__full_rounds_state1_cubed_3 =
      (column8_row62) * (column8_row53);
  const FieldElementT poseidon__poseidon__full_rounds_state2_cubed_3 =
      (column8_row49) * (column8_row61);
  const FieldElementT poseidon__poseidon__partial_rounds_state0_cubed_0 =
      (column5_row0) * (column5_row1);
  const FieldElementT poseidon__poseidon__partial_rounds_state0_cubed_1 =
      (column5_row2) * (column5_row3);
  const FieldElementT poseidon__poseidon__partial_rounds_state0_cubed_2 =
      (column5_row4) * (column5_row5);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_0 =
      (column7_row1) * (column7_row3);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_1 =
      (column7_row5) * (column7_row7);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_2 =
      (column7_row9) * (column7_row11);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_19 =
      (column7_row77) * (column7_row79);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_20 =
      (column7_row81) * (column7_row83);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_21 =
      (column7_row85) * (column7_row87);
  FractionFieldElement<FieldElementT> res(FieldElementT::Zero());
  {
    // Compute a sum of constraints with denominator = domain0.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain3.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/decode/opcode_range_check/bit:
        const FieldElementT constraint =
            ((cpu__decode__opcode_range_check__bit_0) * (cpu__decode__opcode_range_check__bit_0)) -
            (cpu__decode__opcode_range_check__bit_0);
        inner_sum += random_coefficients[0] * constraint;
      }
      outer_sum += inner_sum * domain3;
    }

    {
      // Compute a sum of constraints with numerator = domain20.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for diluted_check/permutation/step0:
        const FieldElementT constraint =
            (((diluted_check__permutation__interaction_elm_) - (column2_row1)) *
             (column10_inter1_row1)) -
            (((diluted_check__permutation__interaction_elm_) - (column1_row1)) *
             (column10_inter1_row0));
        inner_sum += random_coefficients[48] * constraint;
      }
      {
        // Constraint expression for diluted_check/step:
        const FieldElementT constraint =
            (column9_inter1_row1) -
            (((column9_inter1_row0) *
              ((FieldElementT::One()) +
               ((diluted_check__interaction_z_) * ((column2_row1) - (column2_row0))))) +
             (((diluted_check__interaction_alpha_) * ((column2_row1) - (column2_row0))) *
              ((column2_row1) - (column2_row0))));
        inner_sum += random_coefficients[52] * constraint;
      }
      outer_sum += inner_sum * domain20;
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
        // Constraint expression for cpu/decode/opcode_range_check/zero:
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
        // Constraint expression for cpu/decode/opcode_range_check_input:
        const FieldElementT constraint =
            (column3_row1) -
            (((((((column0_row0) * (offset_size_)) + (column6_row4)) * (offset_size_)) +
               (column6_row8)) *
              (offset_size_)) +
             (column6_row0));
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
            ((((cpu__decode__opcode_range_check__bit_0) * (column8_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_range_check__bit_0)) *
               (column8_row0))) +
             (column6_row0));
        inner_sum += random_coefficients[7] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem0_addr:
        const FieldElementT constraint =
            ((column3_row4) + (half_offset_size_)) -
            ((((cpu__decode__opcode_range_check__bit_1) * (column8_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_range_check__bit_1)) *
               (column8_row0))) +
             (column6_row8));
        inner_sum += random_coefficients[8] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem1_addr:
        const FieldElementT constraint =
            ((column3_row12) + (half_offset_size_)) -
            ((((((cpu__decode__opcode_range_check__bit_2) * (column3_row0)) +
                ((cpu__decode__opcode_range_check__bit_4) * (column8_row0))) +
               ((cpu__decode__opcode_range_check__bit_3) * (column8_row8))) +
              ((cpu__decode__flag_op1_base_op0_0) * (column3_row5))) +
             (column6_row4));
        inner_sum += random_coefficients[9] * constraint;
      }
      {
        // Constraint expression for cpu/operands/ops_mul:
        const FieldElementT constraint = (column8_row4) - ((column3_row5) * (column3_row13));
        inner_sum += random_coefficients[10] * constraint;
      }
      {
        // Constraint expression for cpu/operands/res:
        const FieldElementT constraint =
            (((FieldElementT::One()) - (cpu__decode__opcode_range_check__bit_9)) *
             (column8_row12)) -
            ((((cpu__decode__opcode_range_check__bit_5) * ((column3_row5) + (column3_row13))) +
              ((cpu__decode__opcode_range_check__bit_6) * (column8_row4))) +
             ((cpu__decode__flag_res_op1_0) * (column3_row13)));
        inner_sum += random_coefficients[11] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_fp:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_12) * ((column3_row9) - (column8_row8));
        inner_sum += random_coefficients[18] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_pc:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_12) *
            ((column3_row5) - (((column3_row0) + (cpu__decode__opcode_range_check__bit_2)) +
                               (FieldElementT::One())));
        inner_sum += random_coefficients[19] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off0:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_12) * ((column6_row0) - (half_offset_size_));
        inner_sum += random_coefficients[20] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/off1:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_12) *
            ((column6_row8) - ((half_offset_size_) + (FieldElementT::One())));
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
            (((column6_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z))) - (half_offset_size_));
        inner_sum += random_coefficients[23] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/ret/off2:
        const FieldElementT constraint =
            (cpu__decode__opcode_range_check__bit_13) *
            (((column6_row4) + (FieldElementT::One())) - (half_offset_size_));
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
            (cpu__decode__opcode_range_check__bit_14) * ((column3_row9) - (column8_row12));
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
      {
        // Constraint expression for poseidon/poseidon/full_rounds_state0_squaring:
        const FieldElementT constraint = ((column8_row6) * (column8_row6)) - (column8_row9);
        inner_sum += random_coefficients[99] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_rounds_state1_squaring:
        const FieldElementT constraint = ((column8_row14) * (column8_row14)) - (column8_row5);
        inner_sum += random_coefficients[100] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_rounds_state2_squaring:
        const FieldElementT constraint = ((column8_row1) * (column8_row1)) - (column8_row13);
        inner_sum += random_coefficients[101] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain21.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp0:
        const FieldElementT constraint =
            (column8_row2) - ((cpu__decode__opcode_range_check__bit_9) * (column3_row9));
        inner_sum += random_coefficients[12] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp1:
        const FieldElementT constraint = (column8_row10) - ((column8_row2) * (column8_row12));
        inner_sum += random_coefficients[13] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_negative:
        const FieldElementT constraint =
            ((((FieldElementT::One()) - (cpu__decode__opcode_range_check__bit_9)) *
              (column3_row16)) +
             ((column8_row2) * ((column3_row16) - ((column3_row0) + (column3_row13))))) -
            ((((cpu__decode__flag_pc_update_regular_0) * (npc_reg_0)) +
              ((cpu__decode__opcode_range_check__bit_7) * (column8_row12))) +
             ((cpu__decode__opcode_range_check__bit_8) * ((column3_row0) + (column8_row12))));
        inner_sum += random_coefficients[14] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_positive:
        const FieldElementT constraint =
            ((column8_row10) - (cpu__decode__opcode_range_check__bit_9)) *
            ((column3_row16) - (npc_reg_0));
        inner_sum += random_coefficients[15] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_ap/ap_update:
        const FieldElementT constraint =
            (column8_row16) -
            ((((column8_row0) + ((cpu__decode__opcode_range_check__bit_10) * (column8_row12))) +
              (cpu__decode__opcode_range_check__bit_11)) +
             ((cpu__decode__opcode_range_check__bit_12) *
              (FieldElementT::ConstexprFromBigInt(0x2_Z))));
        inner_sum += random_coefficients[16] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_fp/fp_update:
        const FieldElementT constraint =
            (column8_row24) - ((((cpu__decode__fp_update_regular_0) * (column8_row8)) +
                                ((cpu__decode__opcode_range_check__bit_13) * (column3_row9))) +
                               ((cpu__decode__opcode_range_check__bit_12) *
                                ((column8_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z)))));
        inner_sum += random_coefficients[17] * constraint;
      }
      outer_sum += inner_sum * domain21;
    }

    {
      // Compute a sum of constraints with numerator = domain6.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/full_round0:
        const FieldElementT constraint =
            (column8_row22) - ((((((poseidon__poseidon__full_rounds_state0_cubed_0) +
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
            ((column8_row30) + (poseidon__poseidon__full_rounds_state1_cubed_0)) -
            (((poseidon__poseidon__full_rounds_state0_cubed_0) +
              (poseidon__poseidon__full_rounds_state2_cubed_0)) +
             (poseidon__poseidon__full_round_key1));
        inner_sum += random_coefficients[108] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_round2:
        const FieldElementT constraint =
            (((column8_row17) + (poseidon__poseidon__full_rounds_state2_cubed_0)) +
             (poseidon__poseidon__full_rounds_state2_cubed_0)) -
            (((poseidon__poseidon__full_rounds_state0_cubed_0) +
              (poseidon__poseidon__full_rounds_state1_cubed_0)) +
             (poseidon__poseidon__full_round_key2));
        inner_sum += random_coefficients[109] * constraint;
      }
      outer_sum += inner_sum * domain6;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain4);
  }

  {
    // Compute a sum of constraints with denominator = domain22.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for initial_ap:
        const FieldElementT constraint = (column8_row0) - (initial_ap_);
        inner_sum += random_coefficients[27] * constraint;
      }
      {
        // Constraint expression for initial_fp:
        const FieldElementT constraint = (column8_row8) - (initial_ap_);
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
               (column11_inter1_row0)) +
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
        // Constraint expression for range_check16/perm/init0:
        const FieldElementT constraint =
            ((((range_check16__perm__interaction_elm_) - (column6_row2)) * (column11_inter1_row1)) +
             (column6_row0)) -
            (range_check16__perm__interaction_elm_);
        inner_sum += random_coefficients[41] * constraint;
      }
      {
        // Constraint expression for range_check16/minimum:
        const FieldElementT constraint = (column6_row2) - (range_check_min_);
        inner_sum += random_coefficients[45] * constraint;
      }
      {
        // Constraint expression for diluted_check/permutation/init0:
        const FieldElementT constraint =
            ((((diluted_check__permutation__interaction_elm_) - (column2_row0)) *
              (column10_inter1_row0)) +
             (column1_row0)) -
            (diluted_check__permutation__interaction_elm_);
        inner_sum += random_coefficients[47] * constraint;
      }
      {
        // Constraint expression for diluted_check/init:
        const FieldElementT constraint = (column9_inter1_row0) - (FieldElementT::One());
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
        // Constraint expression for range_check_builtin/init_addr:
        const FieldElementT constraint = (column3_row74) - (initial_range_check_addr_);
        inner_sum += random_coefficients[81] * constraint;
      }
      {
        // Constraint expression for bitwise/init_var_pool_addr:
        const FieldElementT constraint = (column3_row26) - (initial_bitwise_addr_);
        inner_sum += random_coefficients[82] * constraint;
      }
      {
        // Constraint expression for poseidon/param_0/init_input_output_addr:
        const FieldElementT constraint = (column3_row6) - (initial_poseidon_addr_);
        inner_sum += random_coefficients[93] * constraint;
      }
      {
        // Constraint expression for poseidon/param_1/init_input_output_addr:
        const FieldElementT constraint =
            (column3_row38) - ((initial_poseidon_addr_) + (FieldElementT::One()));
        inner_sum += random_coefficients[95] * constraint;
      }
      {
        // Constraint expression for poseidon/param_2/init_input_output_addr:
        const FieldElementT constraint =
            (column3_row22) -
            ((initial_poseidon_addr_) + (FieldElementT::ConstexprFromBigInt(0x2_Z)));
        inner_sum += random_coefficients[97] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain22);
  }

  {
    // Compute a sum of constraints with denominator = domain21.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for final_ap:
        const FieldElementT constraint = (column8_row0) - (final_ap_);
        inner_sum += random_coefficients[30] * constraint;
      }
      {
        // Constraint expression for final_fp:
        const FieldElementT constraint = (column8_row8) - (initial_ap_);
        inner_sum += random_coefficients[31] * constraint;
      }
      {
        // Constraint expression for final_pc:
        const FieldElementT constraint = (column3_row0) - (final_pc_);
        inner_sum += random_coefficients[32] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain21);
  }

  {
    // Compute a sum of constraints with denominator = domain1.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain23.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/step0:
        const FieldElementT constraint =
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column4_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column4_row3)))) *
             (column11_inter1_row2)) -
            (((memory__multi_column_perm__perm__interaction_elm_) -
              ((column3_row2) +
               ((memory__multi_column_perm__hash_interaction_elm0_) * (column3_row3)))) *
             (column11_inter1_row0));
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
      outer_sum += inner_sum * domain23;
    }

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/partial_rounds_state0_squaring:
        const FieldElementT constraint = ((column5_row0) * (column5_row0)) - (column5_row1);
        inner_sum += random_coefficients[102] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain9.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/partial_round0:
        const FieldElementT constraint =
            (column5_row6) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__partial_rounds_state0_cubed_0)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column5_row2))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state0_cubed_1))) +
                (column5_row4)) +
               (column5_row4)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state0_cubed_2))) +
             (poseidon__poseidon__partial_round_key0));
        inner_sum += random_coefficients[119] * constraint;
      }
      outer_sum += inner_sum * domain9;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain1);
  }

  {
    // Compute a sum of constraints with denominator = domain23.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for memory/multi_column_perm/perm/last:
        const FieldElementT constraint =
            (column11_inter1_row0) - (memory__multi_column_perm__perm__public_memory_prod_);
        inner_sum += random_coefficients[35] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain23);
  }

  {
    // Compute a sum of constraints with denominator = domain2.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain24.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for range_check16/perm/step0:
        const FieldElementT constraint =
            (((range_check16__perm__interaction_elm_) - (column6_row6)) * (column11_inter1_row5)) -
            (((range_check16__perm__interaction_elm_) - (column6_row4)) * (column11_inter1_row1));
        inner_sum += random_coefficients[42] * constraint;
      }
      {
        // Constraint expression for range_check16/diff_is_bit:
        const FieldElementT constraint =
            ((range_check16__diff_0) * (range_check16__diff_0)) - (range_check16__diff_0);
        inner_sum += random_coefficients[44] * constraint;
      }
      outer_sum += inner_sum * domain24;
    }

    {
      // Compute a sum of constraints with numerator = domain15.
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
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column6_row3) - (pedersen__points__y))) -
            ((column7_row2) * ((column6_row1) - (pedersen__points__x)));
        inner_sum += random_coefficients[63] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column7_row2) * (column7_row2)) -
            ((pedersen__hash0__ec_subset_sum__bit_0) *
             (((column6_row1) + (pedersen__points__x)) + (column6_row5)));
        inner_sum += random_coefficients[64] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column6_row3) + (column6_row7))) -
            ((column7_row2) * ((column6_row1) - (column6_row5)));
        inner_sum += random_coefficients[65] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column6_row5) - (column6_row1));
        inner_sum += random_coefficients[66] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (pedersen__hash0__ec_subset_sum__bit_neg_0) * ((column6_row7) - (column6_row3));
        inner_sum += random_coefficients[67] * constraint;
      }
      outer_sum += inner_sum * domain15;
    }

    {
      // Compute a sum of constraints with numerator = domain11.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/partial_rounds_state1_squaring:
        const FieldElementT constraint = ((column7_row1) * (column7_row1)) - (column7_row3);
        inner_sum += random_coefficients[103] * constraint;
      }
      outer_sum += inner_sum * domain11;
    }

    {
      // Compute a sum of constraints with numerator = domain12.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/partial_round1:
        const FieldElementT constraint =
            (column7_row13) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__partial_rounds_state1_cubed_0)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column7_row5))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state1_cubed_1))) +
                (column7_row9)) +
               (column7_row9)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state1_cubed_2))) +
             (poseidon__poseidon__partial_round_key1));
        inner_sum += random_coefficients[120] * constraint;
      }
      outer_sum += inner_sum * domain12;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain2);
  }

  {
    // Compute a sum of constraints with denominator = domain24.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for range_check16/perm/last:
        const FieldElementT constraint =
            (column11_inter1_row1) - (range_check16__perm__public_memory_prod_);
        inner_sum += random_coefficients[43] * constraint;
      }
      {
        // Constraint expression for range_check16/maximum:
        const FieldElementT constraint = (column6_row2) - (range_check_max_);
        inner_sum += random_coefficients[46] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain24);
  }

  {
    // Compute a sum of constraints with denominator = domain20.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for diluted_check/permutation/last:
        const FieldElementT constraint =
            (column10_inter1_row0) - (diluted_check__permutation__public_memory_prod_);
        inner_sum += random_coefficients[49] * constraint;
      }
      {
        // Constraint expression for diluted_check/last:
        const FieldElementT constraint = (column9_inter1_row0) - (diluted_check__final_cum_val_);
        inner_sum += random_coefficients[53] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain20);
  }

  {
    // Compute a sum of constraints with denominator = domain16.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column7_row89) * ((column7_row0) - ((column7_row4) + (column7_row4)));
        inner_sum += random_coefficients[54] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column7_row89) *
            ((column7_row4) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column7_row768)));
        inner_sum += random_coefficients[55] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column7_row89) -
            ((column7_row1022) * ((column7_row768) - ((column7_row772) + (column7_row772))));
        inner_sum += random_coefficients[56] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column7_row1022) *
            ((column7_row772) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column7_row784)));
        inner_sum += random_coefficients[57] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column7_row1022) - (((column7_row1004) - ((column7_row1008) + (column7_row1008))) *
                                 ((column7_row784) - ((column7_row788) + (column7_row788))));
        inner_sum += random_coefficients[58] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column7_row1004) - ((column7_row1008) + (column7_row1008))) *
            ((column7_row788) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column7_row1004)));
        inner_sum += random_coefficients[59] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain18.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/copy_point/x:
        const FieldElementT constraint = (column6_row1025) - (column6_row1021);
        inner_sum += random_coefficients[68] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/copy_point/y:
        const FieldElementT constraint = (column6_row1027) - (column6_row1023);
        inner_sum += random_coefficients[69] * constraint;
      }
      outer_sum += inner_sum * domain18;
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
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column7_row0;
        inner_sum += random_coefficients[61] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain17);
  }

  {
    // Compute a sum of constraints with denominator = domain15.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column7_row0;
        inner_sum += random_coefficients[62] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain15);
  }

  {
    // Compute a sum of constraints with denominator = domain19.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/hash0/init/x:
        const FieldElementT constraint = (column6_row1) - ((pedersen__shift_point_).x);
        inner_sum += random_coefficients[70] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/init/y:
        const FieldElementT constraint = (column6_row3) - ((pedersen__shift_point_).y);
        inner_sum += random_coefficients[71] * constraint;
      }
      {
        // Constraint expression for pedersen/input0_value0:
        const FieldElementT constraint = (column3_row11) - (column7_row0);
        inner_sum += random_coefficients[72] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value0:
        const FieldElementT constraint = (column3_row1035) - (column7_row1024);
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
        const FieldElementT constraint = (column3_row523) - (column6_row2045);
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
      // Compute a sum of constraints with numerator = domain25.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input0_addr:
        const FieldElementT constraint =
            (column3_row2058) - ((column3_row522) + (FieldElementT::One()));
        inner_sum += random_coefficients[73] * constraint;
      }
      outer_sum += inner_sum * domain25;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain19);
  }

  {
    // Compute a sum of constraints with denominator = domain13.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for range_check_builtin/value:
        const FieldElementT constraint = (range_check_builtin__value7_0) - (column3_row75);
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
      {
        // Constraint expression for poseidon/poseidon/add_first_round_key0:
        const FieldElementT constraint =
            ((column3_row7) +
             (FieldElementT::ConstexprFromBigInt(
                 0x6861759ea556a2339dd92f9562a30b9e58e2ad98109ae4780b7fd8eac77fe6f_Z))) -
            (column8_row6);
        inner_sum += random_coefficients[104] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/add_first_round_key1:
        const FieldElementT constraint =
            ((column3_row39) +
             (FieldElementT::ConstexprFromBigInt(
                 0x3827681995d5af9ffc8397a3d00425a3da43f76abf28a64e4ab1a22f27508c4_Z))) -
            (column8_row14);
        inner_sum += random_coefficients[105] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/add_first_round_key2:
        const FieldElementT constraint =
            ((column3_row23) +
             (FieldElementT::ConstexprFromBigInt(
                 0x3a3956d2fad44d0e7f760a2277dc7cb2cac75dc279b2d687a0dbe17704a8309_Z))) -
            (column8_row1);
        inner_sum += random_coefficients[106] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/last_full_round0:
        const FieldElementT constraint =
            (column3_row71) - (((((poseidon__poseidon__full_rounds_state0_cubed_7) +
                                  (poseidon__poseidon__full_rounds_state0_cubed_7)) +
                                 (poseidon__poseidon__full_rounds_state0_cubed_7)) +
                                (poseidon__poseidon__full_rounds_state1_cubed_7)) +
                               (poseidon__poseidon__full_rounds_state2_cubed_7));
        inner_sum += random_coefficients[110] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/last_full_round1:
        const FieldElementT constraint =
            ((column3_row103) + (poseidon__poseidon__full_rounds_state1_cubed_7)) -
            ((poseidon__poseidon__full_rounds_state0_cubed_7) +
             (poseidon__poseidon__full_rounds_state2_cubed_7));
        inner_sum += random_coefficients[111] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/last_full_round2:
        const FieldElementT constraint =
            (((column3_row87) + (poseidon__poseidon__full_rounds_state2_cubed_7)) +
             (poseidon__poseidon__full_rounds_state2_cubed_7)) -
            ((poseidon__poseidon__full_rounds_state0_cubed_7) +
             (poseidon__poseidon__full_rounds_state1_cubed_7));
        inner_sum += random_coefficients[112] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/copy_partial_rounds0_i0:
        const FieldElementT constraint = (column5_row122) - (column7_row1);
        inner_sum += random_coefficients[113] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/copy_partial_rounds0_i1:
        const FieldElementT constraint = (column5_row124) - (column7_row5);
        inner_sum += random_coefficients[114] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/copy_partial_rounds0_i2:
        const FieldElementT constraint = (column5_row126) - (column7_row9);
        inner_sum += random_coefficients[115] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_full_to_partial0:
        const FieldElementT constraint =
            (((column5_row0) + (poseidon__poseidon__full_rounds_state2_cubed_3)) +
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
            (column5_row2) -
            ((((((FieldElementT::ConstexprFromBigInt(
                     0x800000000000010fffffffffffffffffffffffffffffffffffffffffffffffd_Z)) *
                 (poseidon__poseidon__full_rounds_state1_cubed_3)) +
                ((FieldElementT::ConstexprFromBigInt(0xa_Z)) *
                 (poseidon__poseidon__full_rounds_state2_cubed_3))) +
               ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column5_row0))) +
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
            (column5_row4) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__full_rounds_state2_cubed_3)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column5_row0))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state0_cubed_0))) +
                (column5_row2)) +
               (column5_row2)) +
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
            (column8_row70) -
            (((((((FieldElementT::ConstexprFromBigInt(0x10_Z)) *
                  (poseidon__poseidon__partial_rounds_state1_cubed_19)) +
                 ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column7_row81))) +
                ((FieldElementT::ConstexprFromBigInt(0x10_Z)) *
                 (poseidon__poseidon__partial_rounds_state1_cubed_20))) +
               ((FieldElementT::ConstexprFromBigInt(0x6_Z)) * (column7_row85))) +
              (poseidon__poseidon__partial_rounds_state1_cubed_21)) +
             (FieldElementT::ConstexprFromBigInt(
                 0x13d1b5cfd87693224f0ac561ab2c15ca53365d768311af59cefaf701bc53b37_Z)));
        inner_sum += random_coefficients[121] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_partial_to_full1:
        const FieldElementT constraint =
            (column8_row78) -
            ((((((FieldElementT::ConstexprFromBigInt(0x4_Z)) *
                 (poseidon__poseidon__partial_rounds_state1_cubed_20)) +
                (column7_row85)) +
               (column7_row85)) +
              (poseidon__poseidon__partial_rounds_state1_cubed_21)) +
             (FieldElementT::ConstexprFromBigInt(
                 0x3195d6b2d930e71cede286d5b8b41d49296ddf222bcd3bf3717a12a9a6947ff_Z)));
        inner_sum += random_coefficients[122] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_partial_to_full2:
        const FieldElementT constraint =
            (column8_row65) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__partial_rounds_state1_cubed_19)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column7_row81))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state1_cubed_20))) +
                (column7_row85)) +
               (column7_row85)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state1_cubed_21))) +
             (FieldElementT::ConstexprFromBigInt(
                 0x2c14fccabc26929170cc7ac9989c823608b9008bef3b8e16b6089a5d33cd72e_Z)));
        inner_sum += random_coefficients[123] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain26.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for range_check_builtin/addr_step:
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
      outer_sum += inner_sum * domain26;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain13);
  }

  {
    // Compute a sum of constraints with denominator = domain5.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain10.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/step_var_pool_addr:
        const FieldElementT constraint =
            (column3_row58) - ((column3_row26) + (FieldElementT::One()));
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
            ((bitwise__sum_var_0_0) + (bitwise__sum_var_8_0)) - (column3_row27);
        inner_sum += random_coefficients[86] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain5);
  }

  {
    // Compute a sum of constraints with denominator = domain14.
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
    res += FractionFieldElement<FieldElementT>(outer_sum, domain14);
  }

  {
    // Compute a sum of constraints with denominator = domain7.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain27.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/param_0/addr_input_output_step:
        const FieldElementT constraint =
            (column3_row70) - ((column3_row6) + (FieldElementT::ConstexprFromBigInt(0x3_Z)));
        inner_sum += random_coefficients[94] * constraint;
      }
      {
        // Constraint expression for poseidon/param_1/addr_input_output_step:
        const FieldElementT constraint =
            (column3_row102) - ((column3_row38) + (FieldElementT::ConstexprFromBigInt(0x3_Z)));
        inner_sum += random_coefficients[96] * constraint;
      }
      {
        // Constraint expression for poseidon/param_2/addr_input_output_step:
        const FieldElementT constraint =
            (column3_row86) - ((column3_row22) + (FieldElementT::ConstexprFromBigInt(0x3_Z)));
        inner_sum += random_coefficients[98] * constraint;
      }
      outer_sum += inner_sum * domain27;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain7);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> CpuAirDefinition<FieldElementT, 7>::DomainEvalsAtPoint(
    gsl::span<const FieldElementT> point_powers,
    [[maybe_unused]] gsl::span<const FieldElementT> shifts) const {
  [[maybe_unused]] const FieldElementT& point = point_powers[0];
  const FieldElementT& domain0 = (point_powers[1]) - (FieldElementT::One());
  const FieldElementT& domain1 = (point_powers[2]) - (FieldElementT::One());
  const FieldElementT& domain2 = (point_powers[3]) - (FieldElementT::One());
  const FieldElementT& domain3 = (point_powers[4]) - (shifts[0]);
  const FieldElementT& domain4 = (point_powers[4]) - (FieldElementT::One());
  const FieldElementT& domain5 = (point_powers[5]) - (FieldElementT::One());
  const FieldElementT& domain6 = (point_powers[6]) - (shifts[1]);
  const FieldElementT& domain7 = (point_powers[6]) - (FieldElementT::One());
  const FieldElementT& domain8 = (point_powers[7]) - (shifts[2]);
  const FieldElementT& domain9 =
      ((point_powers[7]) - (shifts[3])) * ((point_powers[7]) - (shifts[4])) * (domain8);
  const FieldElementT& domain10 = (point_powers[7]) - (shifts[1]);
  const FieldElementT& domain11 =
      ((point_powers[7]) - (shifts[5])) * ((point_powers[7]) - (shifts[6])) *
      ((point_powers[7]) - (shifts[7])) * ((point_powers[7]) - (shifts[8])) *
      ((point_powers[7]) - (shifts[9])) * ((point_powers[7]) - (shifts[10])) *
      ((point_powers[7]) - (shifts[11])) * ((point_powers[7]) - (shifts[0])) * (domain8) *
      (domain10);
  const FieldElementT& domain12 = ((point_powers[7]) - (shifts[12])) *
                                  ((point_powers[7]) - (shifts[13])) *
                                  ((point_powers[7]) - (shifts[14])) * (domain11);
  const FieldElementT& domain13 = (point_powers[7]) - (FieldElementT::One());
  const FieldElementT& domain14 =
      ((point_powers[7]) - (shifts[15])) * ((point_powers[7]) - (shifts[16])) *
      ((point_powers[7]) - (shifts[17])) * ((point_powers[7]) - (shifts[18])) *
      ((point_powers[7]) - (shifts[19])) * ((point_powers[7]) - (shifts[20])) *
      ((point_powers[7]) - (shifts[21])) * ((point_powers[7]) - (shifts[22])) *
      ((point_powers[7]) - (shifts[23])) * ((point_powers[7]) - (shifts[24])) *
      ((point_powers[7]) - (shifts[25])) * ((point_powers[7]) - (shifts[26])) *
      ((point_powers[7]) - (shifts[27])) * ((point_powers[7]) - (shifts[28])) *
      ((point_powers[7]) - (shifts[29])) * (domain13);
  const FieldElementT& domain15 = (point_powers[8]) - (shifts[30]);
  const FieldElementT& domain16 = (point_powers[8]) - (FieldElementT::One());
  const FieldElementT& domain17 = (point_powers[8]) - (shifts[4]);
  const FieldElementT& domain18 = (point_powers[9]) - (shifts[31]);
  const FieldElementT& domain19 = (point_powers[9]) - (FieldElementT::One());
  return {
      domain0,  domain1,  domain2,  domain3,  domain4,  domain5,  domain6,
      domain7,  domain8,  domain9,  domain10, domain11, domain12, domain13,
      domain14, domain15, domain16, domain17, domain18, domain19,
  };
}

template <typename FieldElementT>
std::vector<uint64_t> CpuAirDefinition<FieldElementT, 7>::ParseDynamicParams(
    [[maybe_unused]] const std::map<std::string, uint64_t>& params) const {
  std::vector<uint64_t> result;

  ASSERT_RELEASE(params.size() == kNumDynamicParams, "Inconsistent dynamic data.");
  result.reserve(kNumDynamicParams);
  return result;
}

template <typename FieldElementT>
TraceGenerationContext CpuAirDefinition<FieldElementT, 7>::GetTraceGenerationContext() const {
  TraceGenerationContext ctx;

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 128)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 64)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 64)) - (1)) >= (0), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 64)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(
      IsPowerOfTwo(trace_length_),
      "Coset step (MemberExpression(trace_length)) must be a power of two");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 128)) - (1)) >= (0), "step must not exceed dimension.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 128)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 2048)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 2048)) - (1)) >= (0), "step must not exceed dimension.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 2048)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(((trace_length_) - (1)) >= (0), "Index out of range.");

  ASSERT_RELEASE((trace_length_) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 4)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 4)) - (1)) >= (0), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 4)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 16)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 2)), "Dimension should be a power of 2.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 2)) - (1)) >= (0), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 2)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 16)) - (1)) >= (0), "step must not exceed dimension.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 16)) >= (0), "Index out of range.");

  ASSERT_RELEASE(((trace_length_) - (2)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (3)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (4)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (9)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (5)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (13)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (11)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (7)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (15)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (10)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (6)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (14)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (1023)) >= (0), "Offset must be smaller than trace length.");

  ASSERT_RELEASE(((trace_length_) - (90)) >= (0), "Offset must be smaller than trace length.");

  ctx.AddVirtualColumn(
      "mem_pool/addr", VirtualColumn(/*column=*/kColumn3Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "mem_pool/value", VirtualColumn(/*column=*/kColumn3Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "range_check16_pool", VirtualColumn(/*column=*/kColumn6Column, /*step=*/4, /*row_offset=*/0));
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
      "cpu/decode/opcode_range_check/column",
      VirtualColumn(/*column=*/kColumn0Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/off0", VirtualColumn(/*column=*/kColumn6Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/decode/off1", VirtualColumn(/*column=*/kColumn6Column, /*step=*/16, /*row_offset=*/8));
  ctx.AddVirtualColumn(
      "cpu/decode/off2", VirtualColumn(/*column=*/kColumn6Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/registers/ap", VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/registers/fp", VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/8));
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
      "cpu/operands/ops_mul",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/4));
  ctx.AddVirtualColumn(
      "cpu/operands/res", VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp0",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "cpu/update_registers/update_pc/tmp1",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/10));
  ctx.AddVirtualColumn(
      "memory/sorted/addr", VirtualColumn(/*column=*/kColumn4Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "memory/sorted/value",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "memory/multi_column_perm/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn11Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "orig/public_memory/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "orig/public_memory/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/16, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "range_check16/sorted",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/4, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "range_check16/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn11Inter1Column - kNumColumnsFirst, /*step=*/4, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "diluted_pool", VirtualColumn(/*column=*/kColumn1Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "diluted_check/permuted_values",
      VirtualColumn(/*column=*/kColumn2Column, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "diluted_check/cumulative_value",
      VirtualColumn(
          /*column=*/kColumn9Inter1Column - kNumColumnsFirst, /*step=*/1, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "diluted_check/permutation/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn10Inter1Column - kNumColumnsFirst, /*step=*/1, /*row_offset=*/0));
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
      "pedersen/hash0/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/4, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/4, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/4, /*row_offset=*/2));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/4, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/1024, /*row_offset=*/1022));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/1024, /*row_offset=*/89));
  ctx.AddVirtualColumn(
      "range_check_builtin/mem/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/74));
  ctx.AddVirtualColumn(
      "range_check_builtin/mem/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/128, /*row_offset=*/75));
  ctx.AddVirtualColumn(
      "range_check_builtin/inner_range_check",
      VirtualColumn(/*column=*/kColumn6Column, /*step=*/16, /*row_offset=*/12));
  ctx.AddVirtualColumn(
      "bitwise/var_pool/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/32, /*row_offset=*/26));
  ctx.AddVirtualColumn(
      "bitwise/var_pool/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/32, /*row_offset=*/27));
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
  ctx.AddVirtualColumn(
      "poseidon/param_0/input_output/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/64, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "poseidon/param_0/input_output/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/64, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "poseidon/param_1/input_output/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/64, /*row_offset=*/38));
  ctx.AddVirtualColumn(
      "poseidon/param_1/input_output/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/64, /*row_offset=*/39));
  ctx.AddVirtualColumn(
      "poseidon/param_2/input_output/addr",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/64, /*row_offset=*/22));
  ctx.AddVirtualColumn(
      "poseidon/param_2/input_output/value",
      VirtualColumn(/*column=*/kColumn3Column, /*step=*/64, /*row_offset=*/23));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state0",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state1",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/14));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state2",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state0_squared",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state1_squared",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state2_squared",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state0",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state1",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/4, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state0_squared",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/2, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state1_squared",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/4, /*row_offset=*/3));

  ctx.AddPeriodicColumn(
      "pedersen/points/x",
      VirtualColumn(/*column=*/kPedersenPointsXPeriodicColumn, /*step=*/4, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "pedersen/points/y",
      VirtualColumn(/*column=*/kPedersenPointsYPeriodicColumn, /*step=*/4, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/full_round_key0",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonFullRoundKey0PeriodicColumn, /*step=*/16, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/full_round_key1",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonFullRoundKey1PeriodicColumn, /*step=*/16, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/full_round_key2",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonFullRoundKey2PeriodicColumn, /*step=*/16, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/partial_round_key0",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonPartialRoundKey0PeriodicColumn, /*step=*/2,
          /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/partial_round_key1",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonPartialRoundKey1PeriodicColumn, /*step=*/4,
          /*row_offset=*/0));

  ctx.AddObject<std::vector<size_t>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);

  return ctx;
}

template <typename FieldElementT>
std::vector<std::pair<int64_t, uint64_t>> CpuAirDefinition<FieldElementT, 7>::GetMask() const {
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
  mask.emplace_back(6, kColumn3Column);
  mask.emplace_back(7, kColumn3Column);
  mask.emplace_back(8, kColumn3Column);
  mask.emplace_back(9, kColumn3Column);
  mask.emplace_back(10, kColumn3Column);
  mask.emplace_back(11, kColumn3Column);
  mask.emplace_back(12, kColumn3Column);
  mask.emplace_back(13, kColumn3Column);
  mask.emplace_back(16, kColumn3Column);
  mask.emplace_back(22, kColumn3Column);
  mask.emplace_back(23, kColumn3Column);
  mask.emplace_back(26, kColumn3Column);
  mask.emplace_back(27, kColumn3Column);
  mask.emplace_back(38, kColumn3Column);
  mask.emplace_back(39, kColumn3Column);
  mask.emplace_back(42, kColumn3Column);
  mask.emplace_back(43, kColumn3Column);
  mask.emplace_back(58, kColumn3Column);
  mask.emplace_back(70, kColumn3Column);
  mask.emplace_back(71, kColumn3Column);
  mask.emplace_back(74, kColumn3Column);
  mask.emplace_back(75, kColumn3Column);
  mask.emplace_back(86, kColumn3Column);
  mask.emplace_back(87, kColumn3Column);
  mask.emplace_back(91, kColumn3Column);
  mask.emplace_back(102, kColumn3Column);
  mask.emplace_back(103, kColumn3Column);
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
  mask.emplace_back(122, kColumn5Column);
  mask.emplace_back(124, kColumn5Column);
  mask.emplace_back(126, kColumn5Column);
  mask.emplace_back(0, kColumn6Column);
  mask.emplace_back(1, kColumn6Column);
  mask.emplace_back(2, kColumn6Column);
  mask.emplace_back(3, kColumn6Column);
  mask.emplace_back(4, kColumn6Column);
  mask.emplace_back(5, kColumn6Column);
  mask.emplace_back(6, kColumn6Column);
  mask.emplace_back(7, kColumn6Column);
  mask.emplace_back(8, kColumn6Column);
  mask.emplace_back(12, kColumn6Column);
  mask.emplace_back(28, kColumn6Column);
  mask.emplace_back(44, kColumn6Column);
  mask.emplace_back(60, kColumn6Column);
  mask.emplace_back(76, kColumn6Column);
  mask.emplace_back(92, kColumn6Column);
  mask.emplace_back(108, kColumn6Column);
  mask.emplace_back(124, kColumn6Column);
  mask.emplace_back(1021, kColumn6Column);
  mask.emplace_back(1023, kColumn6Column);
  mask.emplace_back(1025, kColumn6Column);
  mask.emplace_back(1027, kColumn6Column);
  mask.emplace_back(2045, kColumn6Column);
  mask.emplace_back(0, kColumn7Column);
  mask.emplace_back(1, kColumn7Column);
  mask.emplace_back(2, kColumn7Column);
  mask.emplace_back(3, kColumn7Column);
  mask.emplace_back(4, kColumn7Column);
  mask.emplace_back(5, kColumn7Column);
  mask.emplace_back(7, kColumn7Column);
  mask.emplace_back(9, kColumn7Column);
  mask.emplace_back(11, kColumn7Column);
  mask.emplace_back(13, kColumn7Column);
  mask.emplace_back(77, kColumn7Column);
  mask.emplace_back(79, kColumn7Column);
  mask.emplace_back(81, kColumn7Column);
  mask.emplace_back(83, kColumn7Column);
  mask.emplace_back(85, kColumn7Column);
  mask.emplace_back(87, kColumn7Column);
  mask.emplace_back(89, kColumn7Column);
  mask.emplace_back(768, kColumn7Column);
  mask.emplace_back(772, kColumn7Column);
  mask.emplace_back(784, kColumn7Column);
  mask.emplace_back(788, kColumn7Column);
  mask.emplace_back(1004, kColumn7Column);
  mask.emplace_back(1008, kColumn7Column);
  mask.emplace_back(1022, kColumn7Column);
  mask.emplace_back(1024, kColumn7Column);
  mask.emplace_back(0, kColumn8Column);
  mask.emplace_back(1, kColumn8Column);
  mask.emplace_back(2, kColumn8Column);
  mask.emplace_back(4, kColumn8Column);
  mask.emplace_back(5, kColumn8Column);
  mask.emplace_back(6, kColumn8Column);
  mask.emplace_back(8, kColumn8Column);
  mask.emplace_back(9, kColumn8Column);
  mask.emplace_back(10, kColumn8Column);
  mask.emplace_back(12, kColumn8Column);
  mask.emplace_back(13, kColumn8Column);
  mask.emplace_back(14, kColumn8Column);
  mask.emplace_back(16, kColumn8Column);
  mask.emplace_back(17, kColumn8Column);
  mask.emplace_back(22, kColumn8Column);
  mask.emplace_back(24, kColumn8Column);
  mask.emplace_back(30, kColumn8Column);
  mask.emplace_back(49, kColumn8Column);
  mask.emplace_back(53, kColumn8Column);
  mask.emplace_back(54, kColumn8Column);
  mask.emplace_back(57, kColumn8Column);
  mask.emplace_back(61, kColumn8Column);
  mask.emplace_back(62, kColumn8Column);
  mask.emplace_back(65, kColumn8Column);
  mask.emplace_back(70, kColumn8Column);
  mask.emplace_back(78, kColumn8Column);
  mask.emplace_back(113, kColumn8Column);
  mask.emplace_back(117, kColumn8Column);
  mask.emplace_back(118, kColumn8Column);
  mask.emplace_back(121, kColumn8Column);
  mask.emplace_back(125, kColumn8Column);
  mask.emplace_back(126, kColumn8Column);
  mask.emplace_back(0, kColumn9Inter1Column);
  mask.emplace_back(1, kColumn9Inter1Column);
  mask.emplace_back(0, kColumn10Inter1Column);
  mask.emplace_back(1, kColumn10Inter1Column);
  mask.emplace_back(0, kColumn11Inter1Column);
  mask.emplace_back(1, kColumn11Inter1Column);
  mask.emplace_back(2, kColumn11Inter1Column);
  mask.emplace_back(5, kColumn11Inter1Column);

  return mask;
}

}  // namespace cpu

}  // namespace starkware
