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
CpuAirDefinition<FieldElementT, 6>::CreateCompositionPolynomial(
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
      SafeDiv(trace_length_, 1024),
      SafeDiv(trace_length_, 16384),
      SafeDiv(trace_length_, 32768)};
  const std::vector<uint64_t> gen_exponents = {
      SafeDiv((15) * (trace_length_), 16),
      SafeDiv((255) * (trace_length_), 256),
      SafeDiv((63) * (trace_length_), 64),
      SafeDiv((3) * (trace_length_), 4),
      SafeDiv((31) * (trace_length_), 32),
      SafeDiv((7) * (trace_length_), 8),
      SafeDiv((11) * (trace_length_), 16),
      SafeDiv((23) * (trace_length_), 32),
      SafeDiv((25) * (trace_length_), 32),
      SafeDiv((13) * (trace_length_), 16),
      SafeDiv((27) * (trace_length_), 32),
      SafeDiv((29) * (trace_length_), 32),
      SafeDiv((5) * (trace_length_), 8),
      SafeDiv((19) * (trace_length_), 32),
      SafeDiv((21) * (trace_length_), 32),
      SafeDiv((61) * (trace_length_), 64),
      SafeDiv(trace_length_, 2),
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
      (256) * ((SafeDiv(trace_length_, 256)) - (1)),
      (512) * ((SafeDiv(trace_length_, 512)) - (1)),
      (32768) * ((SafeDiv(trace_length_, 32768)) - (1)),
      (16384) * ((SafeDiv(trace_length_, 16384)) - (1)),
      (1024) * ((SafeDiv(trace_length_, 1024)) - (1))};

  BuildAutoPeriodicColumns(gen, &builder);

  BuildPeriodicColumns(gen, &builder);

  return builder.BuildUniquePtr(
      UseOwned(this), gen, trace_length_, random_coefficients.As<FieldElementT>(), point_exponents,
      BatchPow(gen, gen_exponents));
}

template <typename FieldElementT>
void CpuAirDefinition<FieldElementT, 6>::BuildAutoPeriodicColumns(
    const FieldElementT& gen, Builder* builder) const {
  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonFullRoundKey0PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 64),
      kPoseidonPoseidonFullRoundKey0PeriodicColumn);
  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonFullRoundKey1PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 64),
      kPoseidonPoseidonFullRoundKey1PeriodicColumn);
  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonFullRoundKey2PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 64),
      kPoseidonPoseidonFullRoundKey2PeriodicColumn);
  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonPartialRoundKey0PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 8),
      kPoseidonPoseidonPartialRoundKey0PeriodicColumn);
  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(
          kPoseidonPoseidonPartialRoundKey1PeriodicColumnData, gen, FieldElementT::One(),
          this->trace_length_, 16),
      kPoseidonPoseidonPartialRoundKey1PeriodicColumn);
};

template <typename FieldElementT>
std::vector<std::vector<FieldElementT>>
CpuAirDefinition<FieldElementT, 6>::PrecomputeDomainEvalsOnCoset(
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
      FieldElementT::UninitializedVector(256),   FieldElementT::UninitializedVector(256),
      FieldElementT::UninitializedVector(512),   FieldElementT::UninitializedVector(512),
      FieldElementT::UninitializedVector(512),   FieldElementT::UninitializedVector(512),
      FieldElementT::UninitializedVector(512),   FieldElementT::UninitializedVector(512),
      FieldElementT::UninitializedVector(512),   FieldElementT::UninitializedVector(512),
      FieldElementT::UninitializedVector(512),   FieldElementT::UninitializedVector(1024),
      FieldElementT::UninitializedVector(1024),  FieldElementT::UninitializedVector(1024),
      FieldElementT::UninitializedVector(16384), FieldElementT::UninitializedVector(16384),
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

  period = 256;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[11][i] = (point_powers[7][i & (255)]) - (shifts[3]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[12][i] = (point_powers[8][i & (511)]) - (shifts[4]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[13][i] = ((point_powers[8][i & (511)]) - (shifts[3])) *
                                   ((point_powers[8][i & (511)]) - (shifts[5]));
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[14][i] = ((point_powers[8][i & (511)]) - (shifts[6])) *
                                   ((point_powers[8][i & (511)]) - (shifts[7])) *
                                   ((point_powers[8][i & (511)]) - (shifts[8])) *
                                   ((point_powers[8][i & (511)]) - (shifts[9])) *
                                   ((point_powers[8][i & (511)]) - (shifts[10])) *
                                   ((point_powers[8][i & (511)]) - (shifts[11])) *
                                   ((point_powers[8][i & (511)]) - (shifts[0])) *
                                   (precomp_domains[12][i & (512 - 1)]) *
                                   (precomp_domains[13][i & (512 - 1)]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[15][i] = (point_powers[8][i & (511)]) - (shifts[12]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[16][i] = ((point_powers[8][i & (511)]) - (shifts[13])) *
                                   ((point_powers[8][i & (511)]) - (shifts[14])) *
                                   (precomp_domains[14][i & (512 - 1)]) *
                                   (precomp_domains[15][i & (512 - 1)]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[17][i] = ((point_powers[8][i & (511)]) - (shifts[15])) *
                                   ((point_powers[8][i & (511)]) - (shifts[2])) *
                                   (precomp_domains[12][i & (512 - 1)]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[18][i] = (point_powers[8][i & (511)]) - (shifts[16]);
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[19][i] = (point_powers[8][i & (511)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 512;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[20][i] =
              (precomp_domains[13][i & (512 - 1)]) * (precomp_domains[15][i & (512 - 1)]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[21][i] = (point_powers[9][i & (1023)]) - (shifts[3]);
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[22][i] = (point_powers[9][i & (1023)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 1024;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[23][i] = ((point_powers[9][i & (1023)]) - (shifts[17])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[18])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[19])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[20])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[21])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[22])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[23])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[24])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[25])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[26])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[27])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[28])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[29])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[30])) *
                                   ((point_powers[9][i & (1023)]) - (shifts[31])) *
                                   (precomp_domains[22][i & (1024 - 1)]);
        }
      },
      period, kTaskSize);

  period = 16384;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[24][i] = (point_powers[10][i & (16383)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 16384;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[25][i] = (point_powers[10][i & (16383)]) - (shifts[32]);
        }
      },
      period, kTaskSize);

  period = 16384;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[26][i] = (point_powers[10][i & (16383)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);

  period = 16384;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[27][i] = (point_powers[10][i & (16383)]) - (shifts[2]);
        }
      },
      period, kTaskSize);

  period = 32768;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[28][i] = (point_powers[11][i & (32767)]) - (shifts[1]);
        }
      },
      period, kTaskSize);

  period = 32768;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[29][i] = (point_powers[11][i & (32767)]) - (shifts[32]);
        }
      },
      period, kTaskSize);

  period = 32768;
  ASSERT_RELEASE(period < kPeriodUpperBound, "Precomp evals: large dynamic size.");
  task_manager.ParallelFor(
      period,
      [&](const TaskInfo& task_info) {
        for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
          precomp_domains[30][i] = (point_powers[11][i & (32767)]) - (FieldElementT::One());
        }
      },
      period, kTaskSize);
  return precomp_domains;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> CpuAirDefinition<FieldElementT, 6>::ConstraintsEval(
    gsl::span<const FieldElementT> neighbors, gsl::span<const FieldElementT> periodic_columns,
    gsl::span<const FieldElementT> random_coefficients, const FieldElementT& point,
    gsl::span<const FieldElementT> shifts, gsl::span<const FieldElementT> precomp_domains) const {
  ASSERT_VERIFIER(shifts.size() == 42, "shifts should contain 42 elements.");

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
  // domain11 = point^(trace_length / 256) - gen^(3 * trace_length / 4).
  const FieldElementT& domain11 = precomp_domains[11];
  // domain14 = (point^(trace_length / 512) - gen^(11 * trace_length / 16)) * (point^(trace_length /
  // 512) - gen^(23 * trace_length / 32)) * (point^(trace_length / 512) - gen^(25 * trace_length /
  // 32)) * (point^(trace_length / 512) - gen^(13 * trace_length / 16)) * (point^(trace_length /
  // 512) - gen^(27 * trace_length / 32)) * (point^(trace_length / 512) - gen^(29 * trace_length /
  // 32)) * (point^(trace_length / 512) - gen^(15 * trace_length / 16)) * domain12 * domain13.
  const FieldElementT& domain14 = precomp_domains[14];
  // domain16 = (point^(trace_length / 512) - gen^(19 * trace_length / 32)) * (point^(trace_length /
  // 512) - gen^(21 * trace_length / 32)) * domain14 * domain15.
  const FieldElementT& domain16 = precomp_domains[16];
  // domain17 = (point^(trace_length / 512) - gen^(61 * trace_length / 64)) * (point^(trace_length /
  // 512) - gen^(63 * trace_length / 64)) * domain12.
  const FieldElementT& domain17 = precomp_domains[17];
  // domain18 = point^(trace_length / 512) - gen^(trace_length / 2).
  const FieldElementT& domain18 = precomp_domains[18];
  // domain19 = point^(trace_length / 512) - 1.
  const FieldElementT& domain19 = precomp_domains[19];
  // domain20 = domain13 * domain15.
  const FieldElementT& domain20 = precomp_domains[20];
  // domain21 = point^(trace_length / 1024) - gen^(3 * trace_length / 4).
  const FieldElementT& domain21 = precomp_domains[21];
  // domain22 = point^(trace_length / 1024) - 1.
  const FieldElementT& domain22 = precomp_domains[22];
  // domain23 = (point^(trace_length / 1024) - gen^(trace_length / 64)) * (point^(trace_length /
  // 1024) - gen^(trace_length / 32)) * (point^(trace_length / 1024) - gen^(3 * trace_length / 64))
  // * (point^(trace_length / 1024) - gen^(trace_length / 16)) * (point^(trace_length / 1024) -
  // gen^(5 * trace_length / 64)) * (point^(trace_length / 1024) - gen^(3 * trace_length / 32)) *
  // (point^(trace_length / 1024) - gen^(7 * trace_length / 64)) * (point^(trace_length / 1024) -
  // gen^(trace_length / 8)) * (point^(trace_length / 1024) - gen^(9 * trace_length / 64)) *
  // (point^(trace_length / 1024) - gen^(5 * trace_length / 32)) * (point^(trace_length / 1024) -
  // gen^(11 * trace_length / 64)) * (point^(trace_length / 1024) - gen^(3 * trace_length / 16)) *
  // (point^(trace_length / 1024) - gen^(13 * trace_length / 64)) * (point^(trace_length / 1024) -
  // gen^(7 * trace_length / 32)) * (point^(trace_length / 1024) - gen^(15 * trace_length / 64)) *
  // domain22.
  const FieldElementT& domain23 = precomp_domains[23];
  // domain24 = point^(trace_length / 16384) - gen^(255 * trace_length / 256).
  const FieldElementT& domain24 = precomp_domains[24];
  // domain25 = point^(trace_length / 16384) - gen^(251 * trace_length / 256).
  const FieldElementT& domain25 = precomp_domains[25];
  // domain26 = point^(trace_length / 16384) - 1.
  const FieldElementT& domain26 = precomp_domains[26];
  // domain27 = point^(trace_length / 16384) - gen^(63 * trace_length / 64).
  const FieldElementT& domain27 = precomp_domains[27];
  // domain28 = point^(trace_length / 32768) - gen^(255 * trace_length / 256).
  const FieldElementT& domain28 = precomp_domains[28];
  // domain29 = point^(trace_length / 32768) - gen^(251 * trace_length / 256).
  const FieldElementT& domain29 = precomp_domains[29];
  // domain30 = point^(trace_length / 32768) - 1.
  const FieldElementT& domain30 = precomp_domains[30];
  // domain31 = point - gen^(16 * (trace_length / 16 - 1)).
  const FieldElementT& domain31 = (point) - (shifts[33]);
  // domain32 = point - 1.
  const FieldElementT& domain32 = (point) - (FieldElementT::One());
  // domain33 = point - gen^(2 * (trace_length / 2 - 1)).
  const FieldElementT& domain33 = (point) - (shifts[34]);
  // domain34 = point - gen^(8 * (trace_length / 8 - 1)).
  const FieldElementT& domain34 = (point) - (shifts[35]);
  // domain35 = point - gen^(4 * (trace_length / 4 - 1)).
  const FieldElementT& domain35 = (point) - (shifts[36]);
  // domain36 = point - gen^(256 * (trace_length / 256 - 1)).
  const FieldElementT& domain36 = (point) - (shifts[37]);
  // domain37 = point - gen^(512 * (trace_length / 512 - 1)).
  const FieldElementT& domain37 = (point) - (shifts[38]);
  // domain38 = point - gen^(32768 * (trace_length / 32768 - 1)).
  const FieldElementT& domain38 = (point) - (shifts[39]);
  // domain39 = point - gen^(16384 * (trace_length / 16384 - 1)).
  const FieldElementT& domain39 = (point) - (shifts[40]);
  // domain40 = point - gen^(1024 * (trace_length / 1024 - 1)).
  const FieldElementT& domain40 = (point) - (shifts[41]);

  ASSERT_VERIFIER(neighbors.size() == 269, "Neighbors must contain 269 elements.");
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
  const FieldElementT& column5_row38 = neighbors[kColumn5Row38Neighbor];
  const FieldElementT& column5_row39 = neighbors[kColumn5Row39Neighbor];
  const FieldElementT& column5_row70 = neighbors[kColumn5Row70Neighbor];
  const FieldElementT& column5_row71 = neighbors[kColumn5Row71Neighbor];
  const FieldElementT& column5_row102 = neighbors[kColumn5Row102Neighbor];
  const FieldElementT& column5_row103 = neighbors[kColumn5Row103Neighbor];
  const FieldElementT& column5_row134 = neighbors[kColumn5Row134Neighbor];
  const FieldElementT& column5_row135 = neighbors[kColumn5Row135Neighbor];
  const FieldElementT& column5_row167 = neighbors[kColumn5Row167Neighbor];
  const FieldElementT& column5_row198 = neighbors[kColumn5Row198Neighbor];
  const FieldElementT& column5_row199 = neighbors[kColumn5Row199Neighbor];
  const FieldElementT& column5_row231 = neighbors[kColumn5Row231Neighbor];
  const FieldElementT& column5_row262 = neighbors[kColumn5Row262Neighbor];
  const FieldElementT& column5_row263 = neighbors[kColumn5Row263Neighbor];
  const FieldElementT& column5_row295 = neighbors[kColumn5Row295Neighbor];
  const FieldElementT& column5_row326 = neighbors[kColumn5Row326Neighbor];
  const FieldElementT& column5_row358 = neighbors[kColumn5Row358Neighbor];
  const FieldElementT& column5_row359 = neighbors[kColumn5Row359Neighbor];
  const FieldElementT& column5_row390 = neighbors[kColumn5Row390Neighbor];
  const FieldElementT& column5_row391 = neighbors[kColumn5Row391Neighbor];
  const FieldElementT& column5_row454 = neighbors[kColumn5Row454Neighbor];
  const FieldElementT& column5_row518 = neighbors[kColumn5Row518Neighbor];
  const FieldElementT& column5_row550 = neighbors[kColumn5Row550Neighbor];
  const FieldElementT& column5_row711 = neighbors[kColumn5Row711Neighbor];
  const FieldElementT& column5_row902 = neighbors[kColumn5Row902Neighbor];
  const FieldElementT& column5_row903 = neighbors[kColumn5Row903Neighbor];
  const FieldElementT& column5_row966 = neighbors[kColumn5Row966Neighbor];
  const FieldElementT& column5_row967 = neighbors[kColumn5Row967Neighbor];
  const FieldElementT& column5_row1222 = neighbors[kColumn5Row1222Neighbor];
  const FieldElementT& column5_row2438 = neighbors[kColumn5Row2438Neighbor];
  const FieldElementT& column5_row2439 = neighbors[kColumn5Row2439Neighbor];
  const FieldElementT& column5_row4486 = neighbors[kColumn5Row4486Neighbor];
  const FieldElementT& column5_row4487 = neighbors[kColumn5Row4487Neighbor];
  const FieldElementT& column5_row6534 = neighbors[kColumn5Row6534Neighbor];
  const FieldElementT& column5_row6535 = neighbors[kColumn5Row6535Neighbor];
  const FieldElementT& column5_row8582 = neighbors[kColumn5Row8582Neighbor];
  const FieldElementT& column5_row8583 = neighbors[kColumn5Row8583Neighbor];
  const FieldElementT& column5_row10630 = neighbors[kColumn5Row10630Neighbor];
  const FieldElementT& column5_row10631 = neighbors[kColumn5Row10631Neighbor];
  const FieldElementT& column5_row12678 = neighbors[kColumn5Row12678Neighbor];
  const FieldElementT& column5_row12679 = neighbors[kColumn5Row12679Neighbor];
  const FieldElementT& column5_row14726 = neighbors[kColumn5Row14726Neighbor];
  const FieldElementT& column5_row14727 = neighbors[kColumn5Row14727Neighbor];
  const FieldElementT& column5_row16774 = neighbors[kColumn5Row16774Neighbor];
  const FieldElementT& column5_row16775 = neighbors[kColumn5Row16775Neighbor];
  const FieldElementT& column5_row24966 = neighbors[kColumn5Row24966Neighbor];
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
  const FieldElementT& column7_row19 = neighbors[kColumn7Row19Neighbor];
  const FieldElementT& column7_row23 = neighbors[kColumn7Row23Neighbor];
  const FieldElementT& column7_row27 = neighbors[kColumn7Row27Neighbor];
  const FieldElementT& column7_row33 = neighbors[kColumn7Row33Neighbor];
  const FieldElementT& column7_row44 = neighbors[kColumn7Row44Neighbor];
  const FieldElementT& column7_row49 = neighbors[kColumn7Row49Neighbor];
  const FieldElementT& column7_row65 = neighbors[kColumn7Row65Neighbor];
  const FieldElementT& column7_row76 = neighbors[kColumn7Row76Neighbor];
  const FieldElementT& column7_row81 = neighbors[kColumn7Row81Neighbor];
  const FieldElementT& column7_row97 = neighbors[kColumn7Row97Neighbor];
  const FieldElementT& column7_row108 = neighbors[kColumn7Row108Neighbor];
  const FieldElementT& column7_row113 = neighbors[kColumn7Row113Neighbor];
  const FieldElementT& column7_row129 = neighbors[kColumn7Row129Neighbor];
  const FieldElementT& column7_row140 = neighbors[kColumn7Row140Neighbor];
  const FieldElementT& column7_row145 = neighbors[kColumn7Row145Neighbor];
  const FieldElementT& column7_row161 = neighbors[kColumn7Row161Neighbor];
  const FieldElementT& column7_row172 = neighbors[kColumn7Row172Neighbor];
  const FieldElementT& column7_row177 = neighbors[kColumn7Row177Neighbor];
  const FieldElementT& column7_row193 = neighbors[kColumn7Row193Neighbor];
  const FieldElementT& column7_row204 = neighbors[kColumn7Row204Neighbor];
  const FieldElementT& column7_row209 = neighbors[kColumn7Row209Neighbor];
  const FieldElementT& column7_row225 = neighbors[kColumn7Row225Neighbor];
  const FieldElementT& column7_row236 = neighbors[kColumn7Row236Neighbor];
  const FieldElementT& column7_row241 = neighbors[kColumn7Row241Neighbor];
  const FieldElementT& column7_row257 = neighbors[kColumn7Row257Neighbor];
  const FieldElementT& column7_row265 = neighbors[kColumn7Row265Neighbor];
  const FieldElementT& column7_row491 = neighbors[kColumn7Row491Neighbor];
  const FieldElementT& column7_row499 = neighbors[kColumn7Row499Neighbor];
  const FieldElementT& column7_row507 = neighbors[kColumn7Row507Neighbor];
  const FieldElementT& column7_row513 = neighbors[kColumn7Row513Neighbor];
  const FieldElementT& column7_row521 = neighbors[kColumn7Row521Neighbor];
  const FieldElementT& column7_row705 = neighbors[kColumn7Row705Neighbor];
  const FieldElementT& column7_row721 = neighbors[kColumn7Row721Neighbor];
  const FieldElementT& column7_row737 = neighbors[kColumn7Row737Neighbor];
  const FieldElementT& column7_row753 = neighbors[kColumn7Row753Neighbor];
  const FieldElementT& column7_row769 = neighbors[kColumn7Row769Neighbor];
  const FieldElementT& column7_row777 = neighbors[kColumn7Row777Neighbor];
  const FieldElementT& column7_row961 = neighbors[kColumn7Row961Neighbor];
  const FieldElementT& column7_row977 = neighbors[kColumn7Row977Neighbor];
  const FieldElementT& column7_row993 = neighbors[kColumn7Row993Neighbor];
  const FieldElementT& column7_row1009 = neighbors[kColumn7Row1009Neighbor];
  const FieldElementT& column8_row0 = neighbors[kColumn8Row0Neighbor];
  const FieldElementT& column8_row1 = neighbors[kColumn8Row1Neighbor];
  const FieldElementT& column8_row2 = neighbors[kColumn8Row2Neighbor];
  const FieldElementT& column8_row3 = neighbors[kColumn8Row3Neighbor];
  const FieldElementT& column8_row4 = neighbors[kColumn8Row4Neighbor];
  const FieldElementT& column8_row5 = neighbors[kColumn8Row5Neighbor];
  const FieldElementT& column8_row6 = neighbors[kColumn8Row6Neighbor];
  const FieldElementT& column8_row7 = neighbors[kColumn8Row7Neighbor];
  const FieldElementT& column8_row8 = neighbors[kColumn8Row8Neighbor];
  const FieldElementT& column8_row9 = neighbors[kColumn8Row9Neighbor];
  const FieldElementT& column8_row10 = neighbors[kColumn8Row10Neighbor];
  const FieldElementT& column8_row11 = neighbors[kColumn8Row11Neighbor];
  const FieldElementT& column8_row12 = neighbors[kColumn8Row12Neighbor];
  const FieldElementT& column8_row13 = neighbors[kColumn8Row13Neighbor];
  const FieldElementT& column8_row14 = neighbors[kColumn8Row14Neighbor];
  const FieldElementT& column8_row16 = neighbors[kColumn8Row16Neighbor];
  const FieldElementT& column8_row17 = neighbors[kColumn8Row17Neighbor];
  const FieldElementT& column8_row19 = neighbors[kColumn8Row19Neighbor];
  const FieldElementT& column8_row21 = neighbors[kColumn8Row21Neighbor];
  const FieldElementT& column8_row22 = neighbors[kColumn8Row22Neighbor];
  const FieldElementT& column8_row24 = neighbors[kColumn8Row24Neighbor];
  const FieldElementT& column8_row25 = neighbors[kColumn8Row25Neighbor];
  const FieldElementT& column8_row27 = neighbors[kColumn8Row27Neighbor];
  const FieldElementT& column8_row29 = neighbors[kColumn8Row29Neighbor];
  const FieldElementT& column8_row30 = neighbors[kColumn8Row30Neighbor];
  const FieldElementT& column8_row33 = neighbors[kColumn8Row33Neighbor];
  const FieldElementT& column8_row35 = neighbors[kColumn8Row35Neighbor];
  const FieldElementT& column8_row37 = neighbors[kColumn8Row37Neighbor];
  const FieldElementT& column8_row38 = neighbors[kColumn8Row38Neighbor];
  const FieldElementT& column8_row41 = neighbors[kColumn8Row41Neighbor];
  const FieldElementT& column8_row43 = neighbors[kColumn8Row43Neighbor];
  const FieldElementT& column8_row45 = neighbors[kColumn8Row45Neighbor];
  const FieldElementT& column8_row46 = neighbors[kColumn8Row46Neighbor];
  const FieldElementT& column8_row49 = neighbors[kColumn8Row49Neighbor];
  const FieldElementT& column8_row51 = neighbors[kColumn8Row51Neighbor];
  const FieldElementT& column8_row53 = neighbors[kColumn8Row53Neighbor];
  const FieldElementT& column8_row54 = neighbors[kColumn8Row54Neighbor];
  const FieldElementT& column8_row57 = neighbors[kColumn8Row57Neighbor];
  const FieldElementT& column8_row59 = neighbors[kColumn8Row59Neighbor];
  const FieldElementT& column8_row61 = neighbors[kColumn8Row61Neighbor];
  const FieldElementT& column8_row65 = neighbors[kColumn8Row65Neighbor];
  const FieldElementT& column8_row69 = neighbors[kColumn8Row69Neighbor];
  const FieldElementT& column8_row71 = neighbors[kColumn8Row71Neighbor];
  const FieldElementT& column8_row73 = neighbors[kColumn8Row73Neighbor];
  const FieldElementT& column8_row77 = neighbors[kColumn8Row77Neighbor];
  const FieldElementT& column8_row81 = neighbors[kColumn8Row81Neighbor];
  const FieldElementT& column8_row85 = neighbors[kColumn8Row85Neighbor];
  const FieldElementT& column8_row89 = neighbors[kColumn8Row89Neighbor];
  const FieldElementT& column8_row91 = neighbors[kColumn8Row91Neighbor];
  const FieldElementT& column8_row97 = neighbors[kColumn8Row97Neighbor];
  const FieldElementT& column8_row101 = neighbors[kColumn8Row101Neighbor];
  const FieldElementT& column8_row105 = neighbors[kColumn8Row105Neighbor];
  const FieldElementT& column8_row109 = neighbors[kColumn8Row109Neighbor];
  const FieldElementT& column8_row113 = neighbors[kColumn8Row113Neighbor];
  const FieldElementT& column8_row117 = neighbors[kColumn8Row117Neighbor];
  const FieldElementT& column8_row123 = neighbors[kColumn8Row123Neighbor];
  const FieldElementT& column8_row155 = neighbors[kColumn8Row155Neighbor];
  const FieldElementT& column8_row187 = neighbors[kColumn8Row187Neighbor];
  const FieldElementT& column8_row195 = neighbors[kColumn8Row195Neighbor];
  const FieldElementT& column8_row205 = neighbors[kColumn8Row205Neighbor];
  const FieldElementT& column8_row219 = neighbors[kColumn8Row219Neighbor];
  const FieldElementT& column8_row221 = neighbors[kColumn8Row221Neighbor];
  const FieldElementT& column8_row237 = neighbors[kColumn8Row237Neighbor];
  const FieldElementT& column8_row245 = neighbors[kColumn8Row245Neighbor];
  const FieldElementT& column8_row253 = neighbors[kColumn8Row253Neighbor];
  const FieldElementT& column8_row269 = neighbors[kColumn8Row269Neighbor];
  const FieldElementT& column8_row301 = neighbors[kColumn8Row301Neighbor];
  const FieldElementT& column8_row309 = neighbors[kColumn8Row309Neighbor];
  const FieldElementT& column8_row310 = neighbors[kColumn8Row310Neighbor];
  const FieldElementT& column8_row318 = neighbors[kColumn8Row318Neighbor];
  const FieldElementT& column8_row326 = neighbors[kColumn8Row326Neighbor];
  const FieldElementT& column8_row334 = neighbors[kColumn8Row334Neighbor];
  const FieldElementT& column8_row342 = neighbors[kColumn8Row342Neighbor];
  const FieldElementT& column8_row350 = neighbors[kColumn8Row350Neighbor];
  const FieldElementT& column8_row451 = neighbors[kColumn8Row451Neighbor];
  const FieldElementT& column8_row461 = neighbors[kColumn8Row461Neighbor];
  const FieldElementT& column8_row477 = neighbors[kColumn8Row477Neighbor];
  const FieldElementT& column8_row493 = neighbors[kColumn8Row493Neighbor];
  const FieldElementT& column8_row501 = neighbors[kColumn8Row501Neighbor];
  const FieldElementT& column8_row509 = neighbors[kColumn8Row509Neighbor];
  const FieldElementT& column8_row12309 = neighbors[kColumn8Row12309Neighbor];
  const FieldElementT& column8_row12373 = neighbors[kColumn8Row12373Neighbor];
  const FieldElementT& column8_row12565 = neighbors[kColumn8Row12565Neighbor];
  const FieldElementT& column8_row12629 = neighbors[kColumn8Row12629Neighbor];
  const FieldElementT& column8_row16085 = neighbors[kColumn8Row16085Neighbor];
  const FieldElementT& column8_row16149 = neighbors[kColumn8Row16149Neighbor];
  const FieldElementT& column8_row16325 = neighbors[kColumn8Row16325Neighbor];
  const FieldElementT& column8_row16331 = neighbors[kColumn8Row16331Neighbor];
  const FieldElementT& column8_row16337 = neighbors[kColumn8Row16337Neighbor];
  const FieldElementT& column8_row16339 = neighbors[kColumn8Row16339Neighbor];
  const FieldElementT& column8_row16355 = neighbors[kColumn8Row16355Neighbor];
  const FieldElementT& column8_row16357 = neighbors[kColumn8Row16357Neighbor];
  const FieldElementT& column8_row16363 = neighbors[kColumn8Row16363Neighbor];
  const FieldElementT& column8_row16369 = neighbors[kColumn8Row16369Neighbor];
  const FieldElementT& column8_row16371 = neighbors[kColumn8Row16371Neighbor];
  const FieldElementT& column8_row16385 = neighbors[kColumn8Row16385Neighbor];
  const FieldElementT& column8_row16417 = neighbors[kColumn8Row16417Neighbor];
  const FieldElementT& column8_row32647 = neighbors[kColumn8Row32647Neighbor];
  const FieldElementT& column8_row32667 = neighbors[kColumn8Row32667Neighbor];
  const FieldElementT& column8_row32715 = neighbors[kColumn8Row32715Neighbor];
  const FieldElementT& column8_row32721 = neighbors[kColumn8Row32721Neighbor];
  const FieldElementT& column8_row32731 = neighbors[kColumn8Row32731Neighbor];
  const FieldElementT& column8_row32747 = neighbors[kColumn8Row32747Neighbor];
  const FieldElementT& column8_row32753 = neighbors[kColumn8Row32753Neighbor];
  const FieldElementT& column8_row32763 = neighbors[kColumn8Row32763Neighbor];
  const FieldElementT& column9_inter1_row0 = neighbors[kColumn9Inter1Row0Neighbor];
  const FieldElementT& column9_inter1_row1 = neighbors[kColumn9Inter1Row1Neighbor];
  const FieldElementT& column9_inter1_row2 = neighbors[kColumn9Inter1Row2Neighbor];
  const FieldElementT& column9_inter1_row3 = neighbors[kColumn9Inter1Row3Neighbor];
  const FieldElementT& column9_inter1_row5 = neighbors[kColumn9Inter1Row5Neighbor];
  const FieldElementT& column9_inter1_row7 = neighbors[kColumn9Inter1Row7Neighbor];
  const FieldElementT& column9_inter1_row11 = neighbors[kColumn9Inter1Row11Neighbor];
  const FieldElementT& column9_inter1_row15 = neighbors[kColumn9Inter1Row15Neighbor];

  ASSERT_VERIFIER(periodic_columns.size() == 9, "periodic_columns should contain 9 elements.");
  const FieldElementT& pedersen__points__x = periodic_columns[kPedersenPointsXPeriodicColumn];
  const FieldElementT& pedersen__points__y = periodic_columns[kPedersenPointsYPeriodicColumn];
  const FieldElementT& ecdsa__generator_points__x =
      periodic_columns[kEcdsaGeneratorPointsXPeriodicColumn];
  const FieldElementT& ecdsa__generator_points__y =
      periodic_columns[kEcdsaGeneratorPointsYPeriodicColumn];
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
  const FieldElementT ecdsa__signature0__doubling_key__x_squared = (column8_row1) * (column8_row1);
  const FieldElementT ecdsa__signature0__exponentiate_generator__bit_0 =
      (column8_row59) - ((column8_row187) + (column8_row187));
  const FieldElementT ecdsa__signature0__exponentiate_generator__bit_neg_0 =
      (FieldElementT::One()) - (ecdsa__signature0__exponentiate_generator__bit_0);
  const FieldElementT ecdsa__signature0__exponentiate_key__bit_0 =
      (column8_row9) - ((column8_row73) + (column8_row73));
  const FieldElementT ecdsa__signature0__exponentiate_key__bit_neg_0 =
      (FieldElementT::One()) - (ecdsa__signature0__exponentiate_key__bit_0);
  const FieldElementT bitwise__sum_var_0_0 =
      (((((((column7_row1) + ((column7_row17) * (FieldElementT::ConstexprFromBigInt(0x2_Z)))) +
           ((column7_row33) * (FieldElementT::ConstexprFromBigInt(0x4_Z)))) +
          ((column7_row49) * (FieldElementT::ConstexprFromBigInt(0x8_Z)))) +
         ((column7_row65) * (FieldElementT::ConstexprFromBigInt(0x10000000000000000_Z)))) +
        ((column7_row81) * (FieldElementT::ConstexprFromBigInt(0x20000000000000000_Z)))) +
       ((column7_row97) * (FieldElementT::ConstexprFromBigInt(0x40000000000000000_Z)))) +
      ((column7_row113) * (FieldElementT::ConstexprFromBigInt(0x80000000000000000_Z)));
  const FieldElementT bitwise__sum_var_8_0 =
      ((((((((column7_row129) *
             (FieldElementT::ConstexprFromBigInt(0x100000000000000000000000000000000_Z))) +
            ((column7_row145) *
             (FieldElementT::ConstexprFromBigInt(0x200000000000000000000000000000000_Z)))) +
           ((column7_row161) *
            (FieldElementT::ConstexprFromBigInt(0x400000000000000000000000000000000_Z)))) +
          ((column7_row177) *
           (FieldElementT::ConstexprFromBigInt(0x800000000000000000000000000000000_Z)))) +
         ((column7_row193) * (FieldElementT::ConstexprFromBigInt(
                                 0x1000000000000000000000000000000000000000000000000_Z)))) +
        ((column7_row209) * (FieldElementT::ConstexprFromBigInt(
                                0x2000000000000000000000000000000000000000000000000_Z)))) +
       ((column7_row225) * (FieldElementT::ConstexprFromBigInt(
                               0x4000000000000000000000000000000000000000000000000_Z)))) +
      ((column7_row241) *
       (FieldElementT::ConstexprFromBigInt(0x8000000000000000000000000000000000000000000000000_Z)));
  const FieldElementT ec_op__doubling_q__x_squared_0 = (column8_row41) * (column8_row41);
  const FieldElementT ec_op__ec_subset_sum__bit_0 =
      (column8_row21) - ((column8_row85) + (column8_row85));
  const FieldElementT ec_op__ec_subset_sum__bit_neg_0 =
      (FieldElementT::One()) - (ec_op__ec_subset_sum__bit_0);
  const FieldElementT poseidon__poseidon__full_rounds_state0_cubed_0 =
      (column8_row53) * (column8_row29);
  const FieldElementT poseidon__poseidon__full_rounds_state1_cubed_0 =
      (column8_row13) * (column8_row61);
  const FieldElementT poseidon__poseidon__full_rounds_state2_cubed_0 =
      (column8_row45) * (column8_row3);
  const FieldElementT poseidon__poseidon__full_rounds_state0_cubed_7 =
      (column8_row501) * (column8_row477);
  const FieldElementT poseidon__poseidon__full_rounds_state1_cubed_7 =
      (column8_row461) * (column8_row509);
  const FieldElementT poseidon__poseidon__full_rounds_state2_cubed_7 =
      (column8_row493) * (column8_row451);
  const FieldElementT poseidon__poseidon__full_rounds_state0_cubed_3 =
      (column8_row245) * (column8_row221);
  const FieldElementT poseidon__poseidon__full_rounds_state1_cubed_3 =
      (column8_row205) * (column8_row253);
  const FieldElementT poseidon__poseidon__full_rounds_state2_cubed_3 =
      (column8_row237) * (column8_row195);
  const FieldElementT poseidon__poseidon__partial_rounds_state0_cubed_0 =
      (column7_row3) * (column7_row7);
  const FieldElementT poseidon__poseidon__partial_rounds_state0_cubed_1 =
      (column7_row11) * (column7_row15);
  const FieldElementT poseidon__poseidon__partial_rounds_state0_cubed_2 =
      (column7_row19) * (column7_row23);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_0 =
      (column8_row6) * (column8_row14);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_1 =
      (column8_row22) * (column8_row30);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_2 =
      (column8_row38) * (column8_row46);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_19 =
      (column8_row310) * (column8_row318);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_20 =
      (column8_row326) * (column8_row334);
  const FieldElementT poseidon__poseidon__partial_rounds_state1_cubed_21 =
      (column8_row342) * (column8_row350);
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
            ((column4_row0) * ((column1_row0) - (pedersen__points__x)));
        inner_sum += random_coefficients[63] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/x:
        const FieldElementT constraint =
            ((column4_row0) * (column4_row0)) -
            ((pedersen__hash0__ec_subset_sum__bit_0) *
             (((column1_row0) + (pedersen__points__x)) + (column1_row1)));
        inner_sum += random_coefficients[64] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((pedersen__hash0__ec_subset_sum__bit_0) * ((column2_row0) + (column2_row1))) -
            ((column4_row0) * ((column1_row0) - (column1_row1)));
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
            ((((cpu__decode__opcode_rc__bit_0) * (column8_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_0)) * (column8_row0))) +
             (column7_row0));
        inner_sum += random_coefficients[7] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem0_addr:
        const FieldElementT constraint =
            ((column5_row4) + (half_offset_size_)) -
            ((((cpu__decode__opcode_rc__bit_1) * (column8_row8)) +
              (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_1)) * (column8_row0))) +
             (column7_row8));
        inner_sum += random_coefficients[8] * constraint;
      }
      {
        // Constraint expression for cpu/operands/mem1_addr:
        const FieldElementT constraint = ((column5_row12) + (half_offset_size_)) -
                                         ((((((cpu__decode__opcode_rc__bit_2) * (column5_row0)) +
                                             ((cpu__decode__opcode_rc__bit_4) * (column8_row0))) +
                                            ((cpu__decode__opcode_rc__bit_3) * (column8_row8))) +
                                           ((cpu__decode__flag_op1_base_op0_0) * (column5_row5))) +
                                          (column7_row4));
        inner_sum += random_coefficients[9] * constraint;
      }
      {
        // Constraint expression for cpu/operands/ops_mul:
        const FieldElementT constraint = (column8_row4) - ((column5_row5) * (column5_row13));
        inner_sum += random_coefficients[10] * constraint;
      }
      {
        // Constraint expression for cpu/operands/res:
        const FieldElementT constraint =
            (((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column8_row12)) -
            ((((cpu__decode__opcode_rc__bit_5) * ((column5_row5) + (column5_row13))) +
              ((cpu__decode__opcode_rc__bit_6) * (column8_row4))) +
             ((cpu__decode__flag_res_op1_0) * (column5_row13)));
        inner_sum += random_coefficients[11] * constraint;
      }
      {
        // Constraint expression for cpu/opcodes/call/push_fp:
        const FieldElementT constraint =
            (cpu__decode__opcode_rc__bit_12) * ((column5_row9) - (column8_row8));
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
            (cpu__decode__opcode_rc__bit_14) * ((column5_row9) - (column8_row12));
        inner_sum += random_coefficients[26] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain31.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for cpu/update_registers/update_pc/tmp0:
        const FieldElementT constraint =
            (column8_row2) - ((cpu__decode__opcode_rc__bit_9) * (column5_row9));
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
            ((((FieldElementT::One()) - (cpu__decode__opcode_rc__bit_9)) * (column5_row16)) +
             ((column8_row2) * ((column5_row16) - ((column5_row0) + (column5_row13))))) -
            ((((cpu__decode__flag_pc_update_regular_0) * (npc_reg_0)) +
              ((cpu__decode__opcode_rc__bit_7) * (column8_row12))) +
             ((cpu__decode__opcode_rc__bit_8) * ((column5_row0) + (column8_row12))));
        inner_sum += random_coefficients[14] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_pc/pc_cond_positive:
        const FieldElementT constraint =
            ((column8_row10) - (cpu__decode__opcode_rc__bit_9)) * ((column5_row16) - (npc_reg_0));
        inner_sum += random_coefficients[15] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_ap/ap_update:
        const FieldElementT constraint =
            (column8_row16) -
            ((((column8_row0) + ((cpu__decode__opcode_rc__bit_10) * (column8_row12))) +
              (cpu__decode__opcode_rc__bit_11)) +
             ((cpu__decode__opcode_rc__bit_12) * (FieldElementT::ConstexprFromBigInt(0x2_Z))));
        inner_sum += random_coefficients[16] * constraint;
      }
      {
        // Constraint expression for cpu/update_registers/update_fp/fp_update:
        const FieldElementT constraint =
            (column8_row24) - ((((cpu__decode__fp_update_regular_0) * (column8_row8)) +
                                ((cpu__decode__opcode_rc__bit_13) * (column5_row9))) +
                               ((cpu__decode__opcode_rc__bit_12) *
                                ((column8_row0) + (FieldElementT::ConstexprFromBigInt(0x2_Z)))));
        inner_sum += random_coefficients[17] * constraint;
      }
      outer_sum += inner_sum * domain31;
    }

    {
      // Compute a sum of constraints with numerator = domain14.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/partial_rounds_state1_squaring:
        const FieldElementT constraint = ((column8_row6) * (column8_row6)) - (column8_row14);
        inner_sum += random_coefficients[174] * constraint;
      }
      outer_sum += inner_sum * domain14;
    }

    {
      // Compute a sum of constraints with numerator = domain16.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/partial_round1:
        const FieldElementT constraint =
            (column8_row54) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__partial_rounds_state1_cubed_0)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column8_row22))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state1_cubed_1))) +
                (column8_row38)) +
               (column8_row38)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state1_cubed_2))) +
             (poseidon__poseidon__partial_round_key1));
        inner_sum += random_coefficients[191] * constraint;
      }
      outer_sum += inner_sum * domain16;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain5);
  }

  {
    // Compute a sum of constraints with denominator = domain32.
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
        // Constraint expression for diluted_check/permutation/init0:
        const FieldElementT constraint =
            ((((diluted_check__permutation__interaction_elm_) - (column7_row5)) *
              (column9_inter1_row7)) +
             (column7_row1)) -
            (diluted_check__permutation__interaction_elm_);
        inner_sum += random_coefficients[47] * constraint;
      }
      {
        // Constraint expression for diluted_check/init:
        const FieldElementT constraint = (column9_inter1_row3) - (FieldElementT::One());
        inner_sum += random_coefficients[50] * constraint;
      }
      {
        // Constraint expression for diluted_check/first_element:
        const FieldElementT constraint = (column7_row5) - (diluted_check__first_elm_);
        inner_sum += random_coefficients[51] * constraint;
      }
      {
        // Constraint expression for pedersen/init_addr:
        const FieldElementT constraint = (column5_row6) - (initial_pedersen_addr_);
        inner_sum += random_coefficients[74] * constraint;
      }
      {
        // Constraint expression for rc_builtin/init_addr:
        const FieldElementT constraint = (column5_row70) - (initial_rc_addr_);
        inner_sum += random_coefficients[81] * constraint;
      }
      {
        // Constraint expression for ecdsa/init_addr:
        const FieldElementT constraint = (column5_row390) - (initial_ecdsa_addr_);
        inner_sum += random_coefficients[118] * constraint;
      }
      {
        // Constraint expression for bitwise/init_var_pool_addr:
        const FieldElementT constraint = (column5_row198) - (initial_bitwise_addr_);
        inner_sum += random_coefficients[123] * constraint;
      }
      {
        // Constraint expression for ec_op/init_addr:
        const FieldElementT constraint = (column5_row8582) - (initial_ec_op_addr_);
        inner_sum += random_coefficients[134] * constraint;
      }
      {
        // Constraint expression for poseidon/init_input_output_addr:
        const FieldElementT constraint = (column5_row38) - (initial_poseidon_addr_);
        inner_sum += random_coefficients[167] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain32);
  }

  {
    // Compute a sum of constraints with denominator = domain31.
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
        const FieldElementT constraint = (column5_row0) - (final_pc_);
        inner_sum += random_coefficients[32] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain31);
  }

  {
    // Compute a sum of constraints with denominator = domain1.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain33.
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
      outer_sum += inner_sum * domain33;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain1);
  }

  {
    // Compute a sum of constraints with denominator = domain33.
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
    res += FractionFieldElement<FieldElementT>(outer_sum, domain33);
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
      {
        // Constraint expression for poseidon/poseidon/partial_rounds_state0_squaring:
        const FieldElementT constraint = ((column7_row3) * (column7_row3)) - (column7_row7);
        inner_sum += random_coefficients[173] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain34.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for diluted_check/permutation/step0:
        const FieldElementT constraint =
            (((diluted_check__permutation__interaction_elm_) - (column7_row13)) *
             (column9_inter1_row15)) -
            (((diluted_check__permutation__interaction_elm_) - (column7_row9)) *
             (column9_inter1_row7));
        inner_sum += random_coefficients[48] * constraint;
      }
      {
        // Constraint expression for diluted_check/step:
        const FieldElementT constraint =
            (column9_inter1_row11) -
            (((column9_inter1_row3) *
              ((FieldElementT::One()) +
               ((diluted_check__interaction_z_) * ((column7_row13) - (column7_row5))))) +
             (((diluted_check__interaction_alpha_) * ((column7_row13) - (column7_row5))) *
              ((column7_row13) - (column7_row5))));
        inner_sum += random_coefficients[52] * constraint;
      }
      outer_sum += inner_sum * domain34;
    }

    {
      // Compute a sum of constraints with numerator = domain17.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/partial_round0:
        const FieldElementT constraint =
            (column7_row27) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__partial_rounds_state0_cubed_0)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column7_row11))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state0_cubed_1))) +
                (column7_row19)) +
               (column7_row19)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state0_cubed_2))) +
             (poseidon__poseidon__partial_round_key0));
        inner_sum += random_coefficients[190] * constraint;
      }
      outer_sum += inner_sum * domain17;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain3);
  }

  {
    // Compute a sum of constraints with denominator = domain2.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain35.
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
      outer_sum += inner_sum * domain35;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain2);
  }

  {
    // Compute a sum of constraints with denominator = domain35.
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
    res += FractionFieldElement<FieldElementT>(outer_sum, domain35);
  }

  {
    // Compute a sum of constraints with denominator = domain34.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for diluted_check/permutation/last:
        const FieldElementT constraint =
            (column9_inter1_row7) - (diluted_check__permutation__public_memory_prod_);
        inner_sum += random_coefficients[49] * constraint;
      }
      {
        // Constraint expression for diluted_check/last:
        const FieldElementT constraint = (column9_inter1_row3) - (diluted_check__final_cum_val_);
        inner_sum += random_coefficients[53] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain34);
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
            (column8_row71) * ((column3_row0) - ((column3_row1) + (column3_row1)));
        inner_sum += random_coefficients[54] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column8_row71) *
            ((column3_row1) - ((FieldElementT::ConstexprFromBigInt(
                                   0x800000000000000000000000000000000000000000000000_Z)) *
                               (column3_row192)));
        inner_sum += random_coefficients[55] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column8_row71) -
            ((column4_row255) * ((column3_row192) - ((column3_row193) + (column3_row193))));
        inner_sum += random_coefficients[56] * constraint;
      }
      {
        // Constraint expression for
        // pedersen/hash0/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column4_row255) *
            ((column3_row193) - ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column3_row196)));
        inner_sum += random_coefficients[57] * constraint;
      }
      {
        // Constraint expression for pedersen/hash0/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column4_row255) - (((column3_row251) - ((column3_row252) + (column3_row252))) *
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
        // Constraint expression for rc_builtin/value:
        const FieldElementT constraint = (rc_builtin__value7_0) - (column5_row71);
        inner_sum += random_coefficients[79] * constraint;
      }
      {
        // Constraint expression for bitwise/partition:
        const FieldElementT constraint =
            ((bitwise__sum_var_0_0) + (bitwise__sum_var_8_0)) - (column5_row199);
        inner_sum += random_coefficients[127] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain18.
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
      outer_sum += inner_sum * domain18;
    }

    {
      // Compute a sum of constraints with numerator = domain36.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for rc_builtin/addr_step:
        const FieldElementT constraint =
            (column5_row326) - ((column5_row70) + (FieldElementT::One()));
        inner_sum += random_coefficients[80] * constraint;
      }
      outer_sum += inner_sum * domain36;
    }

    {
      // Compute a sum of constraints with numerator = domain21.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/step_var_pool_addr:
        const FieldElementT constraint =
            (column5_row454) - ((column5_row198) + (FieldElementT::One()));
        inner_sum += random_coefficients[124] * constraint;
      }
      outer_sum += inner_sum * domain21;
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
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain8);
  }

  {
    // Compute a sum of constraints with denominator = domain19.
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
        // Constraint expression for pedersen/input0_value0:
        const FieldElementT constraint = (column5_row7) - (column3_row0);
        inner_sum += random_coefficients[72] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_value0:
        const FieldElementT constraint = (column5_row263) - (column3_row256);
        inner_sum += random_coefficients[75] * constraint;
      }
      {
        // Constraint expression for pedersen/input1_addr:
        const FieldElementT constraint =
            (column5_row262) - ((column5_row6) + (FieldElementT::One()));
        inner_sum += random_coefficients[76] * constraint;
      }
      {
        // Constraint expression for pedersen/output_value0:
        const FieldElementT constraint = (column5_row135) - (column1_row511);
        inner_sum += random_coefficients[77] * constraint;
      }
      {
        // Constraint expression for pedersen/output_addr:
        const FieldElementT constraint =
            (column5_row134) - ((column5_row262) + (FieldElementT::One()));
        inner_sum += random_coefficients[78] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/add_first_round_key0:
        const FieldElementT constraint =
            ((column5_row39) +
             (FieldElementT::ConstexprFromBigInt(
                 0x6861759ea556a2339dd92f9562a30b9e58e2ad98109ae4780b7fd8eac77fe6f_Z))) -
            (column8_row53);
        inner_sum += random_coefficients[175] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/add_first_round_key1:
        const FieldElementT constraint =
            ((column5_row103) +
             (FieldElementT::ConstexprFromBigInt(
                 0x3827681995d5af9ffc8397a3d00425a3da43f76abf28a64e4ab1a22f27508c4_Z))) -
            (column8_row13);
        inner_sum += random_coefficients[176] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/add_first_round_key2:
        const FieldElementT constraint =
            ((column5_row167) +
             (FieldElementT::ConstexprFromBigInt(
                 0x3a3956d2fad44d0e7f760a2277dc7cb2cac75dc279b2d687a0dbe17704a8309_Z))) -
            (column8_row45);
        inner_sum += random_coefficients[177] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/last_full_round0:
        const FieldElementT constraint =
            (column5_row231) - (((((poseidon__poseidon__full_rounds_state0_cubed_7) +
                                   (poseidon__poseidon__full_rounds_state0_cubed_7)) +
                                  (poseidon__poseidon__full_rounds_state0_cubed_7)) +
                                 (poseidon__poseidon__full_rounds_state1_cubed_7)) +
                                (poseidon__poseidon__full_rounds_state2_cubed_7));
        inner_sum += random_coefficients[181] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/last_full_round1:
        const FieldElementT constraint =
            ((column5_row295) + (poseidon__poseidon__full_rounds_state1_cubed_7)) -
            ((poseidon__poseidon__full_rounds_state0_cubed_7) +
             (poseidon__poseidon__full_rounds_state2_cubed_7));
        inner_sum += random_coefficients[182] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/last_full_round2:
        const FieldElementT constraint =
            (((column5_row359) + (poseidon__poseidon__full_rounds_state2_cubed_7)) +
             (poseidon__poseidon__full_rounds_state2_cubed_7)) -
            ((poseidon__poseidon__full_rounds_state0_cubed_7) +
             (poseidon__poseidon__full_rounds_state1_cubed_7));
        inner_sum += random_coefficients[183] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/copy_partial_rounds0_i0:
        const FieldElementT constraint = (column7_row491) - (column8_row6);
        inner_sum += random_coefficients[184] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/copy_partial_rounds0_i1:
        const FieldElementT constraint = (column7_row499) - (column8_row22);
        inner_sum += random_coefficients[185] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/copy_partial_rounds0_i2:
        const FieldElementT constraint = (column7_row507) - (column8_row38);
        inner_sum += random_coefficients[186] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_full_to_partial0:
        const FieldElementT constraint =
            (((column7_row3) + (poseidon__poseidon__full_rounds_state2_cubed_3)) +
             (poseidon__poseidon__full_rounds_state2_cubed_3)) -
            (((poseidon__poseidon__full_rounds_state0_cubed_3) +
              (poseidon__poseidon__full_rounds_state1_cubed_3)) +
             (FieldElementT::ConstexprFromBigInt(
                 0x4b085eb1df4258c3453cc97445954bf3433b6ab9dd5a99592864c00f54a3f9a_Z)));
        inner_sum += random_coefficients[187] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_full_to_partial1:
        const FieldElementT constraint =
            (column7_row11) -
            ((((((FieldElementT::ConstexprFromBigInt(
                     0x800000000000010fffffffffffffffffffffffffffffffffffffffffffffffd_Z)) *
                 (poseidon__poseidon__full_rounds_state1_cubed_3)) +
                ((FieldElementT::ConstexprFromBigInt(0xa_Z)) *
                 (poseidon__poseidon__full_rounds_state2_cubed_3))) +
               ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column7_row3))) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state0_cubed_0))) +
             (FieldElementT::ConstexprFromBigInt(
                 0x46fb825257fec76c50fe043684d4e6d2d2f2fdfe9b7c8d7128ca7acc0f66f30_Z)));
        inner_sum += random_coefficients[188] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_full_to_partial2:
        const FieldElementT constraint =
            (column7_row19) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__full_rounds_state2_cubed_3)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column7_row3))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state0_cubed_0))) +
                (column7_row11)) +
               (column7_row11)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state0_cubed_1))) +
             (FieldElementT::ConstexprFromBigInt(
                 0xf2193ba0c7ea33ce6222d9446c1e166202ae5461005292f4a2bcb93420151a_Z)));
        inner_sum += random_coefficients[189] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_partial_to_full0:
        const FieldElementT constraint =
            (column8_row309) -
            (((((((FieldElementT::ConstexprFromBigInt(0x10_Z)) *
                  (poseidon__poseidon__partial_rounds_state1_cubed_19)) +
                 ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column8_row326))) +
                ((FieldElementT::ConstexprFromBigInt(0x10_Z)) *
                 (poseidon__poseidon__partial_rounds_state1_cubed_20))) +
               ((FieldElementT::ConstexprFromBigInt(0x6_Z)) * (column8_row342))) +
              (poseidon__poseidon__partial_rounds_state1_cubed_21)) +
             (FieldElementT::ConstexprFromBigInt(
                 0x13d1b5cfd87693224f0ac561ab2c15ca53365d768311af59cefaf701bc53b37_Z)));
        inner_sum += random_coefficients[192] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_partial_to_full1:
        const FieldElementT constraint =
            (column8_row269) -
            ((((((FieldElementT::ConstexprFromBigInt(0x4_Z)) *
                 (poseidon__poseidon__partial_rounds_state1_cubed_20)) +
                (column8_row342)) +
               (column8_row342)) +
              (poseidon__poseidon__partial_rounds_state1_cubed_21)) +
             (FieldElementT::ConstexprFromBigInt(
                 0x3195d6b2d930e71cede286d5b8b41d49296ddf222bcd3bf3717a12a9a6947ff_Z)));
        inner_sum += random_coefficients[193] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/margin_partial_to_full2:
        const FieldElementT constraint =
            (column8_row301) -
            ((((((((FieldElementT::ConstexprFromBigInt(0x8_Z)) *
                   (poseidon__poseidon__partial_rounds_state1_cubed_19)) +
                  ((FieldElementT::ConstexprFromBigInt(0x4_Z)) * (column8_row326))) +
                 ((FieldElementT::ConstexprFromBigInt(0x6_Z)) *
                  (poseidon__poseidon__partial_rounds_state1_cubed_20))) +
                (column8_row342)) +
               (column8_row342)) +
              ((FieldElementT::ConstexprFromBigInt(
                   0x800000000000010ffffffffffffffffffffffffffffffffffffffffffffffff_Z)) *
               (poseidon__poseidon__partial_rounds_state1_cubed_21))) +
             (FieldElementT::ConstexprFromBigInt(
                 0x2c14fccabc26929170cc7ac9989c823608b9008bef3b8e16b6089a5d33cd72e_Z)));
        inner_sum += random_coefficients[194] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain37.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for pedersen/input0_addr:
        const FieldElementT constraint =
            (column5_row518) - ((column5_row134) + (FieldElementT::One()));
        inner_sum += random_coefficients[73] * constraint;
      }
      {
        // Constraint expression for poseidon/addr_input_output_step_outter:
        const FieldElementT constraint =
            (column5_row550) - ((column5_row358) + (FieldElementT::One()));
        inner_sum += random_coefficients[169] * constraint;
      }
      outer_sum += inner_sum * domain37;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain19);
  }

  {
    // Compute a sum of constraints with denominator = domain6.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain24.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/doubling_key/slope:
        const FieldElementT constraint = ((((ecdsa__signature0__doubling_key__x_squared) +
                                            (ecdsa__signature0__doubling_key__x_squared)) +
                                           (ecdsa__signature0__doubling_key__x_squared)) +
                                          ((ecdsa__sig_config_).alpha)) -
                                         (((column8_row33) + (column8_row33)) * (column8_row35));
        inner_sum += random_coefficients[82] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/doubling_key/x:
        const FieldElementT constraint = ((column8_row35) * (column8_row35)) -
                                         (((column8_row1) + (column8_row1)) + (column8_row65));
        inner_sum += random_coefficients[83] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/doubling_key/y:
        const FieldElementT constraint = ((column8_row33) + (column8_row97)) -
                                         ((column8_row35) * ((column8_row1) - (column8_row65)));
        inner_sum += random_coefficients[84] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/booleanity_test:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_0) *
            ((ecdsa__signature0__exponentiate_key__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[94] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/slope:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_key__bit_0) * ((column8_row49) - (column8_row33))) -
            ((column8_row19) * ((column8_row17) - (column8_row1)));
        inner_sum += random_coefficients[97] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/x:
        const FieldElementT constraint = ((column8_row19) * (column8_row19)) -
                                         ((ecdsa__signature0__exponentiate_key__bit_0) *
                                          (((column8_row17) + (column8_row1)) + (column8_row81)));
        inner_sum += random_coefficients[98] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/y:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_key__bit_0) * ((column8_row49) + (column8_row113))) -
            ((column8_row19) * ((column8_row17) - (column8_row81)));
        inner_sum += random_coefficients[99] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column8_row51) * ((column8_row17) - (column8_row1))) - (FieldElementT::One());
        inner_sum += random_coefficients[100] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/copy_point/x:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_neg_0) * ((column8_row81) - (column8_row17));
        inner_sum += random_coefficients[101] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/copy_point/y:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_key__bit_neg_0) * ((column8_row113) - (column8_row49));
        inner_sum += random_coefficients[102] * constraint;
      }
      {
        // Constraint expression for ec_op/doubling_q/slope:
        const FieldElementT constraint =
            ((((ec_op__doubling_q__x_squared_0) + (ec_op__doubling_q__x_squared_0)) +
              (ec_op__doubling_q__x_squared_0)) +
             ((ec_op__curve_config_).alpha)) -
            (((column8_row25) + (column8_row25)) * (column8_row57));
        inner_sum += random_coefficients[142] * constraint;
      }
      {
        // Constraint expression for ec_op/doubling_q/x:
        const FieldElementT constraint = ((column8_row57) * (column8_row57)) -
                                         (((column8_row41) + (column8_row41)) + (column8_row105));
        inner_sum += random_coefficients[143] * constraint;
      }
      {
        // Constraint expression for ec_op/doubling_q/y:
        const FieldElementT constraint = ((column8_row25) + (column8_row89)) -
                                         ((column8_row57) * ((column8_row41) - (column8_row105)));
        inner_sum += random_coefficients[144] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/booleanity_test:
        const FieldElementT constraint = (ec_op__ec_subset_sum__bit_0) *
                                         ((ec_op__ec_subset_sum__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[153] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/add_points/slope:
        const FieldElementT constraint =
            ((ec_op__ec_subset_sum__bit_0) * ((column8_row37) - (column8_row25))) -
            ((column8_row11) * ((column8_row5) - (column8_row41)));
        inner_sum += random_coefficients[156] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/add_points/x:
        const FieldElementT constraint = ((column8_row11) * (column8_row11)) -
                                         ((ec_op__ec_subset_sum__bit_0) *
                                          (((column8_row5) + (column8_row41)) + (column8_row69)));
        inner_sum += random_coefficients[157] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/add_points/y:
        const FieldElementT constraint =
            ((ec_op__ec_subset_sum__bit_0) * ((column8_row37) + (column8_row101))) -
            ((column8_row11) * ((column8_row5) - (column8_row69)));
        inner_sum += random_coefficients[158] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column8_row43) * ((column8_row5) - (column8_row41))) - (FieldElementT::One());
        inner_sum += random_coefficients[159] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/copy_point/x:
        const FieldElementT constraint =
            (ec_op__ec_subset_sum__bit_neg_0) * ((column8_row69) - (column8_row5));
        inner_sum += random_coefficients[160] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/copy_point/y:
        const FieldElementT constraint =
            (ec_op__ec_subset_sum__bit_neg_0) * ((column8_row101) - (column8_row37));
        inner_sum += random_coefficients[161] * constraint;
      }
      outer_sum += inner_sum * domain24;
    }

    {
      // Compute a sum of constraints with numerator = domain20.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/addr_input_output_step_inner:
        const FieldElementT constraint =
            (column5_row102) - ((column5_row38) + (FieldElementT::One()));
        inner_sum += random_coefficients[168] * constraint;
      }
      outer_sum += inner_sum * domain20;
    }

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/full_rounds_state0_squaring:
        const FieldElementT constraint = ((column8_row53) * (column8_row53)) - (column8_row29);
        inner_sum += random_coefficients[170] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_rounds_state1_squaring:
        const FieldElementT constraint = ((column8_row13) * (column8_row13)) - (column8_row61);
        inner_sum += random_coefficients[171] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_rounds_state2_squaring:
        const FieldElementT constraint = ((column8_row45) * (column8_row45)) - (column8_row3);
        inner_sum += random_coefficients[172] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain11.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for poseidon/poseidon/full_round0:
        const FieldElementT constraint =
            (column8_row117) - ((((((poseidon__poseidon__full_rounds_state0_cubed_0) +
                                    (poseidon__poseidon__full_rounds_state0_cubed_0)) +
                                   (poseidon__poseidon__full_rounds_state0_cubed_0)) +
                                  (poseidon__poseidon__full_rounds_state1_cubed_0)) +
                                 (poseidon__poseidon__full_rounds_state2_cubed_0)) +
                                (poseidon__poseidon__full_round_key0));
        inner_sum += random_coefficients[178] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_round1:
        const FieldElementT constraint =
            ((column8_row77) + (poseidon__poseidon__full_rounds_state1_cubed_0)) -
            (((poseidon__poseidon__full_rounds_state0_cubed_0) +
              (poseidon__poseidon__full_rounds_state2_cubed_0)) +
             (poseidon__poseidon__full_round_key1));
        inner_sum += random_coefficients[179] * constraint;
      }
      {
        // Constraint expression for poseidon/poseidon/full_round2:
        const FieldElementT constraint =
            (((column8_row109) + (poseidon__poseidon__full_rounds_state2_cubed_0)) +
             (poseidon__poseidon__full_rounds_state2_cubed_0)) -
            (((poseidon__poseidon__full_rounds_state0_cubed_0) +
              (poseidon__poseidon__full_rounds_state1_cubed_0)) +
             (poseidon__poseidon__full_round_key2));
        inner_sum += random_coefficients[180] * constraint;
      }
      outer_sum += inner_sum * domain11;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain6);
  }

  {
    // Compute a sum of constraints with denominator = domain7.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = domain28.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/booleanity_test:
        const FieldElementT constraint =
            (ecdsa__signature0__exponentiate_generator__bit_0) *
            ((ecdsa__signature0__exponentiate_generator__bit_0) - (FieldElementT::One()));
        inner_sum += random_coefficients[85] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/slope:
        const FieldElementT constraint =
            ((ecdsa__signature0__exponentiate_generator__bit_0) *
             ((column8_row91) - (ecdsa__generator_points__y))) -
            ((column8_row123) * ((column8_row27) - (ecdsa__generator_points__x)));
        inner_sum += random_coefficients[88] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/x:
        const FieldElementT constraint =
            ((column8_row123) * (column8_row123)) -
            ((ecdsa__signature0__exponentiate_generator__bit_0) *
             (((column8_row27) + (ecdsa__generator_points__x)) + (column8_row155)));
        inner_sum += random_coefficients[89] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/y:
        const FieldElementT constraint = ((ecdsa__signature0__exponentiate_generator__bit_0) *
                                          ((column8_row91) + (column8_row219))) -
                                         ((column8_row123) * ((column8_row27) - (column8_row155)));
        inner_sum += random_coefficients[90] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/add_points/x_diff_inv:
        const FieldElementT constraint =
            ((column8_row7) * ((column8_row27) - (ecdsa__generator_points__x))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[91] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/copy_point/x:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_generator__bit_neg_0) *
                                         ((column8_row155) - (column8_row27));
        inner_sum += random_coefficients[92] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/copy_point/y:
        const FieldElementT constraint = (ecdsa__signature0__exponentiate_generator__bit_neg_0) *
                                         ((column8_row219) - (column8_row91));
        inner_sum += random_coefficients[93] * constraint;
      }
      outer_sum += inner_sum * domain28;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain7);
  }

  {
    // Compute a sum of constraints with denominator = domain29.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/bit_extraction_end:
        const FieldElementT constraint = column8_row59;
        inner_sum += random_coefficients[86] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain29);
  }

  {
    // Compute a sum of constraints with denominator = domain28.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_generator/zeros_tail:
        const FieldElementT constraint = column8_row59;
        inner_sum += random_coefficients[87] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain28);
  }

  {
    // Compute a sum of constraints with denominator = domain25.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/exponentiate_key/bit_extraction_end:
        const FieldElementT constraint = column8_row9;
        inner_sum += random_coefficients[95] * constraint;
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
        // Constraint expression for ecdsa/signature0/exponentiate_key/zeros_tail:
        const FieldElementT constraint = column8_row9;
        inner_sum += random_coefficients[96] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/zeros_tail:
        const FieldElementT constraint = column8_row21;
        inner_sum += random_coefficients[155] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain24);
  }

  {
    // Compute a sum of constraints with denominator = domain30.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/init_gen/x:
        const FieldElementT constraint = (column8_row27) - (((ecdsa__sig_config_).shift_point).x);
        inner_sum += random_coefficients[103] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/init_gen/y:
        const FieldElementT constraint = (column8_row91) + (((ecdsa__sig_config_).shift_point).y);
        inner_sum += random_coefficients[104] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/slope:
        const FieldElementT constraint =
            (column8_row32731) -
            ((column8_row16369) + ((column8_row32763) * ((column8_row32667) - (column8_row16337))));
        inner_sum += random_coefficients[107] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/x:
        const FieldElementT constraint =
            ((column8_row32763) * (column8_row32763)) -
            (((column8_row32667) + (column8_row16337)) + (column8_row16385));
        inner_sum += random_coefficients[108] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/y:
        const FieldElementT constraint =
            ((column8_row32731) + (column8_row16417)) -
            ((column8_row32763) * ((column8_row32667) - (column8_row16385)));
        inner_sum += random_coefficients[109] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/add_results/x_diff_inv:
        const FieldElementT constraint =
            ((column8_row32647) * ((column8_row32667) - (column8_row16337))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[110] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/slope:
        const FieldElementT constraint =
            ((column8_row32753) + (((ecdsa__sig_config_).shift_point).y)) -
            ((column8_row16331) * ((column8_row32721) - (((ecdsa__sig_config_).shift_point).x)));
        inner_sum += random_coefficients[111] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/x:
        const FieldElementT constraint =
            ((column8_row16331) * (column8_row16331)) -
            (((column8_row32721) + (((ecdsa__sig_config_).shift_point).x)) + (column8_row9));
        inner_sum += random_coefficients[112] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/extract_r/x_diff_inv:
        const FieldElementT constraint =
            ((column8_row32715) * ((column8_row32721) - (((ecdsa__sig_config_).shift_point).x))) -
            (FieldElementT::One());
        inner_sum += random_coefficients[113] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/z_nonzero:
        const FieldElementT constraint =
            ((column8_row59) * (column8_row16363)) - (FieldElementT::One());
        inner_sum += random_coefficients[114] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/q_on_curve/x_squared:
        const FieldElementT constraint = (column8_row32747) - ((column8_row1) * (column8_row1));
        inner_sum += random_coefficients[116] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/q_on_curve/on_curve:
        const FieldElementT constraint = ((column8_row33) * (column8_row33)) -
                                         ((((column8_row1) * (column8_row32747)) +
                                           (((ecdsa__sig_config_).alpha) * (column8_row1))) +
                                          ((ecdsa__sig_config_).beta));
        inner_sum += random_coefficients[117] * constraint;
      }
      {
        // Constraint expression for ecdsa/message_addr:
        const FieldElementT constraint =
            (column5_row16774) - ((column5_row390) + (FieldElementT::One()));
        inner_sum += random_coefficients[119] * constraint;
      }
      {
        // Constraint expression for ecdsa/message_value0:
        const FieldElementT constraint = (column5_row16775) - (column8_row59);
        inner_sum += random_coefficients[121] * constraint;
      }
      {
        // Constraint expression for ecdsa/pubkey_value0:
        const FieldElementT constraint = (column5_row391) - (column8_row1);
        inner_sum += random_coefficients[122] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain38.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/pubkey_addr:
        const FieldElementT constraint =
            (column5_row33158) - ((column5_row16774) + (FieldElementT::One()));
        inner_sum += random_coefficients[120] * constraint;
      }
      outer_sum += inner_sum * domain38;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain30);
  }

  {
    // Compute a sum of constraints with denominator = domain26.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ecdsa/signature0/init_key/x:
        const FieldElementT constraint = (column8_row17) - (((ecdsa__sig_config_).shift_point).x);
        inner_sum += random_coefficients[105] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/init_key/y:
        const FieldElementT constraint = (column8_row49) - (((ecdsa__sig_config_).shift_point).y);
        inner_sum += random_coefficients[106] * constraint;
      }
      {
        // Constraint expression for ecdsa/signature0/r_and_w_nonzero:
        const FieldElementT constraint =
            ((column8_row9) * (column8_row16355)) - (FieldElementT::One());
        inner_sum += random_coefficients[115] * constraint;
      }
      {
        // Constraint expression for ec_op/p_y_addr:
        const FieldElementT constraint =
            (column5_row4486) - ((column5_row8582) + (FieldElementT::One()));
        inner_sum += random_coefficients[136] * constraint;
      }
      {
        // Constraint expression for ec_op/q_x_addr:
        const FieldElementT constraint =
            (column5_row12678) - ((column5_row4486) + (FieldElementT::One()));
        inner_sum += random_coefficients[137] * constraint;
      }
      {
        // Constraint expression for ec_op/q_y_addr:
        const FieldElementT constraint =
            (column5_row2438) - ((column5_row12678) + (FieldElementT::One()));
        inner_sum += random_coefficients[138] * constraint;
      }
      {
        // Constraint expression for ec_op/m_addr:
        const FieldElementT constraint =
            (column5_row10630) - ((column5_row2438) + (FieldElementT::One()));
        inner_sum += random_coefficients[139] * constraint;
      }
      {
        // Constraint expression for ec_op/r_x_addr:
        const FieldElementT constraint =
            (column5_row6534) - ((column5_row10630) + (FieldElementT::One()));
        inner_sum += random_coefficients[140] * constraint;
      }
      {
        // Constraint expression for ec_op/r_y_addr:
        const FieldElementT constraint =
            (column5_row14726) - ((column5_row6534) + (FieldElementT::One()));
        inner_sum += random_coefficients[141] * constraint;
      }
      {
        // Constraint expression for ec_op/get_q_x:
        const FieldElementT constraint = (column5_row12679) - (column8_row41);
        inner_sum += random_coefficients[145] * constraint;
      }
      {
        // Constraint expression for ec_op/get_q_y:
        const FieldElementT constraint = (column5_row2439) - (column8_row25);
        inner_sum += random_coefficients[146] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/last_one_is_zero:
        const FieldElementT constraint =
            (column8_row16371) * ((column8_row21) - ((column8_row85) + (column8_row85)));
        inner_sum += random_coefficients[147] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/zeroes_between_ones0:
        const FieldElementT constraint =
            (column8_row16371) *
            ((column8_row85) - ((FieldElementT::ConstexprFromBigInt(
                                    0x800000000000000000000000000000000000000000000000_Z)) *
                                (column8_row12309)));
        inner_sum += random_coefficients[148] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/cumulative_bit192:
        const FieldElementT constraint =
            (column8_row16371) -
            ((column8_row16339) * ((column8_row12309) - ((column8_row12373) + (column8_row12373))));
        inner_sum += random_coefficients[149] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/zeroes_between_ones192:
        const FieldElementT constraint =
            (column8_row16339) *
            ((column8_row12373) -
             ((FieldElementT::ConstexprFromBigInt(0x8_Z)) * (column8_row12565)));
        inner_sum += random_coefficients[150] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/cumulative_bit196:
        const FieldElementT constraint =
            (column8_row16339) - (((column8_row16085) - ((column8_row16149) + (column8_row16149))) *
                                  ((column8_row12565) - ((column8_row12629) + (column8_row12629))));
        inner_sum += random_coefficients[151] * constraint;
      }
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_unpacking/zeroes_between_ones196:
        const FieldElementT constraint =
            ((column8_row16085) - ((column8_row16149) + (column8_row16149))) *
            ((column8_row12629) -
             ((FieldElementT::ConstexprFromBigInt(0x40000000000000_Z)) * (column8_row16085)));
        inner_sum += random_coefficients[152] * constraint;
      }
      {
        // Constraint expression for ec_op/get_m:
        const FieldElementT constraint = (column8_row21) - (column5_row10631);
        inner_sum += random_coefficients[162] * constraint;
      }
      {
        // Constraint expression for ec_op/get_p_x:
        const FieldElementT constraint = (column5_row8583) - (column8_row5);
        inner_sum += random_coefficients[163] * constraint;
      }
      {
        // Constraint expression for ec_op/get_p_y:
        const FieldElementT constraint = (column5_row4487) - (column8_row37);
        inner_sum += random_coefficients[164] * constraint;
      }
      {
        // Constraint expression for ec_op/set_r_x:
        const FieldElementT constraint = (column5_row6535) - (column8_row16325);
        inner_sum += random_coefficients[165] * constraint;
      }
      {
        // Constraint expression for ec_op/set_r_y:
        const FieldElementT constraint = (column5_row14727) - (column8_row16357);
        inner_sum += random_coefficients[166] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain39.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ec_op/p_x_addr:
        const FieldElementT constraint =
            (column5_row24966) - ((column5_row8582) + (FieldElementT::ConstexprFromBigInt(0x7_Z)));
        inner_sum += random_coefficients[135] * constraint;
      }
      outer_sum += inner_sum * domain39;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain26);
  }

  {
    // Compute a sum of constraints with denominator = domain22.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/x_or_y_addr:
        const FieldElementT constraint =
            (column5_row902) - ((column5_row966) + (FieldElementT::One()));
        inner_sum += random_coefficients[125] * constraint;
      }
      {
        // Constraint expression for bitwise/or_is_and_plus_xor:
        const FieldElementT constraint = (column5_row903) - ((column5_row711) + (column5_row967));
        inner_sum += random_coefficients[128] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking192:
        const FieldElementT constraint =
            (((column7_row705) + (column7_row961)) * (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
            (column7_row9);
        inner_sum += random_coefficients[130] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking193:
        const FieldElementT constraint =
            (((column7_row721) + (column7_row977)) * (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
            (column7_row521);
        inner_sum += random_coefficients[131] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking194:
        const FieldElementT constraint =
            (((column7_row737) + (column7_row993)) * (FieldElementT::ConstexprFromBigInt(0x10_Z))) -
            (column7_row265);
        inner_sum += random_coefficients[132] * constraint;
      }
      {
        // Constraint expression for bitwise/unique_unpacking195:
        const FieldElementT constraint = (((column7_row753) + (column7_row1009)) *
                                          (FieldElementT::ConstexprFromBigInt(0x100_Z))) -
                                         (column7_row777);
        inner_sum += random_coefficients[133] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }

    {
      // Compute a sum of constraints with numerator = domain40.
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/next_var_pool_addr:
        const FieldElementT constraint =
            (column5_row1222) - ((column5_row902) + (FieldElementT::One()));
        inner_sum += random_coefficients[126] * constraint;
      }
      outer_sum += inner_sum * domain40;
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain22);
  }

  {
    // Compute a sum of constraints with denominator = domain23.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for bitwise/addition_is_xor_with_and:
        const FieldElementT constraint = ((column7_row1) + (column7_row257)) -
                                         (((column7_row769) + (column7_row513)) + (column7_row513));
        inner_sum += random_coefficients[129] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain23);
  }

  {
    // Compute a sum of constraints with denominator = domain27.
    FieldElementT outer_sum = FieldElementT::Zero();

    {
      // Compute a sum of constraints with numerator = FieldElementT::One().
      FieldElementT inner_sum = FieldElementT::Zero();
      {
        // Constraint expression for ec_op/ec_subset_sum/bit_extraction_end:
        const FieldElementT constraint = column8_row21;
        inner_sum += random_coefficients[154] * constraint;
      }
      outer_sum += inner_sum;  // domain == FieldElementT::One()
    }
    res += FractionFieldElement<FieldElementT>(outer_sum, domain27);
  }
  return res;
}

template <typename FieldElementT>
std::vector<FieldElementT> CpuAirDefinition<FieldElementT, 6>::DomainEvalsAtPoint(
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
  const FieldElementT& domain11 = (point_powers[8]) - (shifts[3]);
  const FieldElementT& domain12 = (point_powers[9]) - (shifts[4]);
  const FieldElementT& domain13 =
      ((point_powers[9]) - (shifts[3])) * ((point_powers[9]) - (shifts[5]));
  const FieldElementT& domain14 =
      ((point_powers[9]) - (shifts[6])) * ((point_powers[9]) - (shifts[7])) *
      ((point_powers[9]) - (shifts[8])) * ((point_powers[9]) - (shifts[9])) *
      ((point_powers[9]) - (shifts[10])) * ((point_powers[9]) - (shifts[11])) *
      ((point_powers[9]) - (shifts[0])) * (domain12) * (domain13);
  const FieldElementT& domain15 = (point_powers[9]) - (shifts[12]);
  const FieldElementT& domain16 = ((point_powers[9]) - (shifts[13])) *
                                  ((point_powers[9]) - (shifts[14])) * (domain14) * (domain15);
  const FieldElementT& domain17 =
      ((point_powers[9]) - (shifts[15])) * ((point_powers[9]) - (shifts[2])) * (domain12);
  const FieldElementT& domain18 = (point_powers[9]) - (shifts[16]);
  const FieldElementT& domain19 = (point_powers[9]) - (FieldElementT::One());
  const FieldElementT& domain20 = (domain13) * (domain15);
  const FieldElementT& domain21 = (point_powers[10]) - (shifts[3]);
  const FieldElementT& domain22 = (point_powers[10]) - (FieldElementT::One());
  const FieldElementT& domain23 =
      ((point_powers[10]) - (shifts[17])) * ((point_powers[10]) - (shifts[18])) *
      ((point_powers[10]) - (shifts[19])) * ((point_powers[10]) - (shifts[20])) *
      ((point_powers[10]) - (shifts[21])) * ((point_powers[10]) - (shifts[22])) *
      ((point_powers[10]) - (shifts[23])) * ((point_powers[10]) - (shifts[24])) *
      ((point_powers[10]) - (shifts[25])) * ((point_powers[10]) - (shifts[26])) *
      ((point_powers[10]) - (shifts[27])) * ((point_powers[10]) - (shifts[28])) *
      ((point_powers[10]) - (shifts[29])) * ((point_powers[10]) - (shifts[30])) *
      ((point_powers[10]) - (shifts[31])) * (domain22);
  const FieldElementT& domain24 = (point_powers[11]) - (shifts[1]);
  const FieldElementT& domain25 = (point_powers[11]) - (shifts[32]);
  const FieldElementT& domain26 = (point_powers[11]) - (FieldElementT::One());
  const FieldElementT& domain27 = (point_powers[11]) - (shifts[2]);
  const FieldElementT& domain28 = (point_powers[12]) - (shifts[1]);
  const FieldElementT& domain29 = (point_powers[12]) - (shifts[32]);
  const FieldElementT& domain30 = (point_powers[12]) - (FieldElementT::One());
  return {
      domain0,  domain1,  domain2,  domain3,  domain4,  domain5,  domain6,  domain7,
      domain8,  domain9,  domain10, domain11, domain12, domain13, domain14, domain15,
      domain16, domain17, domain18, domain19, domain20, domain21, domain22, domain23,
      domain24, domain25, domain26, domain27, domain28, domain29, domain30,
  };
}

template <typename FieldElementT>
TraceGenerationContext CpuAirDefinition<FieldElementT, 6>::GetTraceGenerationContext() const {
  TraceGenerationContext ctx;

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 512)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 512)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 512)) - (1)) <= (SafeDiv(trace_length_, 512)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 512)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 512)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 512)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 512)) <= (SafeDiv(trace_length_, 512)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 512)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 512)), "Index out of range.");

  ASSERT_RELEASE(IsPowerOfTwo(SafeDiv(trace_length_, 16384)), "Dimension should be a power of 2.");

  ASSERT_RELEASE((1) <= (SafeDiv(trace_length_, 16384)), "step must not exceed dimension.");

  ASSERT_RELEASE(
      ((SafeDiv(trace_length_, 16384)) - (1)) <= (SafeDiv(trace_length_, 16384)),
      "Index out of range.");

  ASSERT_RELEASE(((SafeDiv(trace_length_, 16384)) - (1)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) <= (SafeDiv(trace_length_, 16384)), "Index out of range.");

  ASSERT_RELEASE((0) <= ((SafeDiv(trace_length_, 16384)) - (1)), "start must not exceed stop.");

  ASSERT_RELEASE(
      (SafeDiv(trace_length_, 16384)) <= (SafeDiv(trace_length_, 16384)), "Index out of range.");

  ASSERT_RELEASE((SafeDiv(trace_length_, 16384)) >= (0), "Index should be non negative.");

  ASSERT_RELEASE((0) < (SafeDiv(trace_length_, 16384)), "Index out of range.");

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
      "diluted_pool", VirtualColumn(/*column=*/kColumn7Column, /*step=*/8, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "diluted_check/permuted_values",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/8, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state0",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/8, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state0_squared",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/8, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "cpu/registers/ap", VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "cpu/registers/fp", VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/8));
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
      "poseidon/poseidon/partial_rounds_state1",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/6));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/partial_rounds_state1_squared",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16, /*row_offset=*/14));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/key_points/x",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/key_points/y",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/33));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/partial_sum/x",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/17));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/partial_sum/y",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/49));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/selector",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "ec_op/doubled_points/x",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/41));
  ctx.AddVirtualColumn(
      "ec_op/doubled_points/y",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/25));
  ctx.AddVirtualColumn(
      "ec_op/doubling_slope",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/57));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/partial_sum/x",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/5));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/partial_sum/y",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/37));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/selector",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/21));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state0",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/53));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state1",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/13));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state2",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/45));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state0_squared",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/29));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state1_squared",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/61));
  ctx.AddVirtualColumn(
      "poseidon/poseidon/full_rounds_state2_squared",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/doubling_slope",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/35));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/slope",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/19));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_key/x_diff_inv",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/51));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/slope",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/11));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/x_diff_inv",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/64, /*row_offset=*/43));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/partial_sum/x",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/128, /*row_offset=*/27));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/partial_sum/y",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/128, /*row_offset=*/91));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/selector",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/128, /*row_offset=*/59));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/slope",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/128, /*row_offset=*/123));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/exponentiate_generator/x_diff_inv",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/128, /*row_offset=*/7));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn4Column, /*step=*/256, /*row_offset=*/255));
  ctx.AddVirtualColumn(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/256, /*row_offset=*/71));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/r_w_inv",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16384, /*row_offset=*/16355));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/bit_unpacking/prod_ones196",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16384, /*row_offset=*/16339));
  ctx.AddVirtualColumn(
      "ec_op/ec_subset_sum/bit_unpacking/prod_ones192",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/16384, /*row_offset=*/16371));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/add_results_slope",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/32768, /*row_offset=*/32763));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/add_results_inv",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/32768, /*row_offset=*/32647));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/extract_r_slope",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/32768, /*row_offset=*/16331));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/extract_r_inv",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/32768, /*row_offset=*/32715));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/z_inv",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/32768, /*row_offset=*/16363));
  ctx.AddVirtualColumn(
      "ecdsa/signature0/q_x_squared",
      VirtualColumn(/*column=*/kColumn8Column, /*step=*/32768, /*row_offset=*/32747));
  ctx.AddVirtualColumn(
      "memory/multi_column_perm/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn9Inter1Column - kNumColumnsFirst, /*step=*/2, /*row_offset=*/0));
  ctx.AddVirtualColumn(
      "rc16/perm/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn9Inter1Column - kNumColumnsFirst, /*step=*/4, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "diluted_check/cumulative_value",
      VirtualColumn(
          /*column=*/kColumn9Inter1Column - kNumColumnsFirst, /*step=*/8, /*row_offset=*/3));
  ctx.AddVirtualColumn(
      "diluted_check/permutation/cum_prod0",
      VirtualColumn(
          /*column=*/kColumn9Inter1Column - kNumColumnsFirst, /*step=*/8, /*row_offset=*/7));
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
  ctx.AddVirtualColumn(
      "bitwise/x/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1024, /*row_offset=*/198));
  ctx.AddVirtualColumn(
      "bitwise/x/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1024, /*row_offset=*/199));
  ctx.AddVirtualColumn(
      "bitwise/y/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1024, /*row_offset=*/454));
  ctx.AddVirtualColumn(
      "bitwise/y/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1024, /*row_offset=*/455));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1024, /*row_offset=*/710));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1024, /*row_offset=*/711));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1024, /*row_offset=*/966));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1024, /*row_offset=*/967));
  ctx.AddVirtualColumn(
      "bitwise/x_or_y/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1024, /*row_offset=*/902));
  ctx.AddVirtualColumn(
      "bitwise/x_or_y/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/1024, /*row_offset=*/903));
  ctx.AddVirtualColumn(
      "bitwise/diluted_var_pool",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "bitwise/x", VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/1));
  ctx.AddVirtualColumn(
      "bitwise/y", VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/257));
  ctx.AddVirtualColumn(
      "bitwise/x_and_y", VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/513));
  ctx.AddVirtualColumn(
      "bitwise/x_xor_y", VirtualColumn(/*column=*/kColumn7Column, /*step=*/16, /*row_offset=*/769));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking192",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/1024, /*row_offset=*/9));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking193",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/1024, /*row_offset=*/521));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking194",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/1024, /*row_offset=*/265));
  ctx.AddVirtualColumn(
      "bitwise/trim_unpacking195",
      VirtualColumn(/*column=*/kColumn7Column, /*step=*/1024, /*row_offset=*/777));
  ctx.AddVirtualColumn(
      "ec_op/p_x/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/8582));
  ctx.AddVirtualColumn(
      "ec_op/p_x/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/8583));
  ctx.AddVirtualColumn(
      "ec_op/p_y/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/4486));
  ctx.AddVirtualColumn(
      "ec_op/p_y/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/4487));
  ctx.AddVirtualColumn(
      "ec_op/q_x/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/12678));
  ctx.AddVirtualColumn(
      "ec_op/q_x/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/12679));
  ctx.AddVirtualColumn(
      "ec_op/q_y/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/2438));
  ctx.AddVirtualColumn(
      "ec_op/q_y/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/2439));
  ctx.AddVirtualColumn(
      "ec_op/m/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/10630));
  ctx.AddVirtualColumn(
      "ec_op/m/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/10631));
  ctx.AddVirtualColumn(
      "ec_op/r_x/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/6534));
  ctx.AddVirtualColumn(
      "ec_op/r_x/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/6535));
  ctx.AddVirtualColumn(
      "ec_op/r_y/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/14726));
  ctx.AddVirtualColumn(
      "ec_op/r_y/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/16384, /*row_offset=*/14727));
  ctx.AddVirtualColumn(
      "poseidon/input_output/addr",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/64, /*row_offset=*/38));
  ctx.AddVirtualColumn(
      "poseidon/input_output/value",
      VirtualColumn(/*column=*/kColumn5Column, /*step=*/64, /*row_offset=*/39));

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
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/full_round_key0",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonFullRoundKey0PeriodicColumn, /*step=*/64, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/full_round_key1",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonFullRoundKey1PeriodicColumn, /*step=*/64, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/full_round_key2",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonFullRoundKey2PeriodicColumn, /*step=*/64, /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/partial_round_key0",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonPartialRoundKey0PeriodicColumn, /*step=*/8,
          /*row_offset=*/0));
  ctx.AddPeriodicColumn(
      "poseidon/poseidon/partial_round_key1",
      VirtualColumn(
          /*column=*/kPoseidonPoseidonPartialRoundKey1PeriodicColumn, /*step=*/16,
          /*row_offset=*/0));

  ctx.AddObject<std::vector<size_t>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "pedersen/hash0/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);
  ctx.AddObject<std::vector<size_t>>(
      "ec_op/ec_subset_sum/bit_unpacking/ones_indices", {251, 196, 192});
  ctx.AddObject<BigInt<4>>(
      "ec_op/ec_subset_sum/bit_unpacking/limit",
      0x800000000000011000000000000000000000000000000000000000000000001_Z);

  return ctx;
}

template <typename FieldElementT>
std::vector<std::pair<int64_t, uint64_t>> CpuAirDefinition<FieldElementT, 6>::GetMask() const {
  std::vector<std::pair<int64_t, uint64_t>> mask;

  mask.reserve(269);
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
  mask.emplace_back(38, kColumn5Column);
  mask.emplace_back(39, kColumn5Column);
  mask.emplace_back(70, kColumn5Column);
  mask.emplace_back(71, kColumn5Column);
  mask.emplace_back(102, kColumn5Column);
  mask.emplace_back(103, kColumn5Column);
  mask.emplace_back(134, kColumn5Column);
  mask.emplace_back(135, kColumn5Column);
  mask.emplace_back(167, kColumn5Column);
  mask.emplace_back(198, kColumn5Column);
  mask.emplace_back(199, kColumn5Column);
  mask.emplace_back(231, kColumn5Column);
  mask.emplace_back(262, kColumn5Column);
  mask.emplace_back(263, kColumn5Column);
  mask.emplace_back(295, kColumn5Column);
  mask.emplace_back(326, kColumn5Column);
  mask.emplace_back(358, kColumn5Column);
  mask.emplace_back(359, kColumn5Column);
  mask.emplace_back(390, kColumn5Column);
  mask.emplace_back(391, kColumn5Column);
  mask.emplace_back(454, kColumn5Column);
  mask.emplace_back(518, kColumn5Column);
  mask.emplace_back(550, kColumn5Column);
  mask.emplace_back(711, kColumn5Column);
  mask.emplace_back(902, kColumn5Column);
  mask.emplace_back(903, kColumn5Column);
  mask.emplace_back(966, kColumn5Column);
  mask.emplace_back(967, kColumn5Column);
  mask.emplace_back(1222, kColumn5Column);
  mask.emplace_back(2438, kColumn5Column);
  mask.emplace_back(2439, kColumn5Column);
  mask.emplace_back(4486, kColumn5Column);
  mask.emplace_back(4487, kColumn5Column);
  mask.emplace_back(6534, kColumn5Column);
  mask.emplace_back(6535, kColumn5Column);
  mask.emplace_back(8582, kColumn5Column);
  mask.emplace_back(8583, kColumn5Column);
  mask.emplace_back(10630, kColumn5Column);
  mask.emplace_back(10631, kColumn5Column);
  mask.emplace_back(12678, kColumn5Column);
  mask.emplace_back(12679, kColumn5Column);
  mask.emplace_back(14726, kColumn5Column);
  mask.emplace_back(14727, kColumn5Column);
  mask.emplace_back(16774, kColumn5Column);
  mask.emplace_back(16775, kColumn5Column);
  mask.emplace_back(24966, kColumn5Column);
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
  mask.emplace_back(19, kColumn7Column);
  mask.emplace_back(23, kColumn7Column);
  mask.emplace_back(27, kColumn7Column);
  mask.emplace_back(33, kColumn7Column);
  mask.emplace_back(44, kColumn7Column);
  mask.emplace_back(49, kColumn7Column);
  mask.emplace_back(65, kColumn7Column);
  mask.emplace_back(76, kColumn7Column);
  mask.emplace_back(81, kColumn7Column);
  mask.emplace_back(97, kColumn7Column);
  mask.emplace_back(108, kColumn7Column);
  mask.emplace_back(113, kColumn7Column);
  mask.emplace_back(129, kColumn7Column);
  mask.emplace_back(140, kColumn7Column);
  mask.emplace_back(145, kColumn7Column);
  mask.emplace_back(161, kColumn7Column);
  mask.emplace_back(172, kColumn7Column);
  mask.emplace_back(177, kColumn7Column);
  mask.emplace_back(193, kColumn7Column);
  mask.emplace_back(204, kColumn7Column);
  mask.emplace_back(209, kColumn7Column);
  mask.emplace_back(225, kColumn7Column);
  mask.emplace_back(236, kColumn7Column);
  mask.emplace_back(241, kColumn7Column);
  mask.emplace_back(257, kColumn7Column);
  mask.emplace_back(265, kColumn7Column);
  mask.emplace_back(491, kColumn7Column);
  mask.emplace_back(499, kColumn7Column);
  mask.emplace_back(507, kColumn7Column);
  mask.emplace_back(513, kColumn7Column);
  mask.emplace_back(521, kColumn7Column);
  mask.emplace_back(705, kColumn7Column);
  mask.emplace_back(721, kColumn7Column);
  mask.emplace_back(737, kColumn7Column);
  mask.emplace_back(753, kColumn7Column);
  mask.emplace_back(769, kColumn7Column);
  mask.emplace_back(777, kColumn7Column);
  mask.emplace_back(961, kColumn7Column);
  mask.emplace_back(977, kColumn7Column);
  mask.emplace_back(993, kColumn7Column);
  mask.emplace_back(1009, kColumn7Column);
  mask.emplace_back(0, kColumn8Column);
  mask.emplace_back(1, kColumn8Column);
  mask.emplace_back(2, kColumn8Column);
  mask.emplace_back(3, kColumn8Column);
  mask.emplace_back(4, kColumn8Column);
  mask.emplace_back(5, kColumn8Column);
  mask.emplace_back(6, kColumn8Column);
  mask.emplace_back(7, kColumn8Column);
  mask.emplace_back(8, kColumn8Column);
  mask.emplace_back(9, kColumn8Column);
  mask.emplace_back(10, kColumn8Column);
  mask.emplace_back(11, kColumn8Column);
  mask.emplace_back(12, kColumn8Column);
  mask.emplace_back(13, kColumn8Column);
  mask.emplace_back(14, kColumn8Column);
  mask.emplace_back(16, kColumn8Column);
  mask.emplace_back(17, kColumn8Column);
  mask.emplace_back(19, kColumn8Column);
  mask.emplace_back(21, kColumn8Column);
  mask.emplace_back(22, kColumn8Column);
  mask.emplace_back(24, kColumn8Column);
  mask.emplace_back(25, kColumn8Column);
  mask.emplace_back(27, kColumn8Column);
  mask.emplace_back(29, kColumn8Column);
  mask.emplace_back(30, kColumn8Column);
  mask.emplace_back(33, kColumn8Column);
  mask.emplace_back(35, kColumn8Column);
  mask.emplace_back(37, kColumn8Column);
  mask.emplace_back(38, kColumn8Column);
  mask.emplace_back(41, kColumn8Column);
  mask.emplace_back(43, kColumn8Column);
  mask.emplace_back(45, kColumn8Column);
  mask.emplace_back(46, kColumn8Column);
  mask.emplace_back(49, kColumn8Column);
  mask.emplace_back(51, kColumn8Column);
  mask.emplace_back(53, kColumn8Column);
  mask.emplace_back(54, kColumn8Column);
  mask.emplace_back(57, kColumn8Column);
  mask.emplace_back(59, kColumn8Column);
  mask.emplace_back(61, kColumn8Column);
  mask.emplace_back(65, kColumn8Column);
  mask.emplace_back(69, kColumn8Column);
  mask.emplace_back(71, kColumn8Column);
  mask.emplace_back(73, kColumn8Column);
  mask.emplace_back(77, kColumn8Column);
  mask.emplace_back(81, kColumn8Column);
  mask.emplace_back(85, kColumn8Column);
  mask.emplace_back(89, kColumn8Column);
  mask.emplace_back(91, kColumn8Column);
  mask.emplace_back(97, kColumn8Column);
  mask.emplace_back(101, kColumn8Column);
  mask.emplace_back(105, kColumn8Column);
  mask.emplace_back(109, kColumn8Column);
  mask.emplace_back(113, kColumn8Column);
  mask.emplace_back(117, kColumn8Column);
  mask.emplace_back(123, kColumn8Column);
  mask.emplace_back(155, kColumn8Column);
  mask.emplace_back(187, kColumn8Column);
  mask.emplace_back(195, kColumn8Column);
  mask.emplace_back(205, kColumn8Column);
  mask.emplace_back(219, kColumn8Column);
  mask.emplace_back(221, kColumn8Column);
  mask.emplace_back(237, kColumn8Column);
  mask.emplace_back(245, kColumn8Column);
  mask.emplace_back(253, kColumn8Column);
  mask.emplace_back(269, kColumn8Column);
  mask.emplace_back(301, kColumn8Column);
  mask.emplace_back(309, kColumn8Column);
  mask.emplace_back(310, kColumn8Column);
  mask.emplace_back(318, kColumn8Column);
  mask.emplace_back(326, kColumn8Column);
  mask.emplace_back(334, kColumn8Column);
  mask.emplace_back(342, kColumn8Column);
  mask.emplace_back(350, kColumn8Column);
  mask.emplace_back(451, kColumn8Column);
  mask.emplace_back(461, kColumn8Column);
  mask.emplace_back(477, kColumn8Column);
  mask.emplace_back(493, kColumn8Column);
  mask.emplace_back(501, kColumn8Column);
  mask.emplace_back(509, kColumn8Column);
  mask.emplace_back(12309, kColumn8Column);
  mask.emplace_back(12373, kColumn8Column);
  mask.emplace_back(12565, kColumn8Column);
  mask.emplace_back(12629, kColumn8Column);
  mask.emplace_back(16085, kColumn8Column);
  mask.emplace_back(16149, kColumn8Column);
  mask.emplace_back(16325, kColumn8Column);
  mask.emplace_back(16331, kColumn8Column);
  mask.emplace_back(16337, kColumn8Column);
  mask.emplace_back(16339, kColumn8Column);
  mask.emplace_back(16355, kColumn8Column);
  mask.emplace_back(16357, kColumn8Column);
  mask.emplace_back(16363, kColumn8Column);
  mask.emplace_back(16369, kColumn8Column);
  mask.emplace_back(16371, kColumn8Column);
  mask.emplace_back(16385, kColumn8Column);
  mask.emplace_back(16417, kColumn8Column);
  mask.emplace_back(32647, kColumn8Column);
  mask.emplace_back(32667, kColumn8Column);
  mask.emplace_back(32715, kColumn8Column);
  mask.emplace_back(32721, kColumn8Column);
  mask.emplace_back(32731, kColumn8Column);
  mask.emplace_back(32747, kColumn8Column);
  mask.emplace_back(32753, kColumn8Column);
  mask.emplace_back(32763, kColumn8Column);
  mask.emplace_back(0, kColumn9Inter1Column);
  mask.emplace_back(1, kColumn9Inter1Column);
  mask.emplace_back(2, kColumn9Inter1Column);
  mask.emplace_back(3, kColumn9Inter1Column);
  mask.emplace_back(5, kColumn9Inter1Column);
  mask.emplace_back(7, kColumn9Inter1Column);
  mask.emplace_back(11, kColumn9Inter1Column);
  mask.emplace_back(15, kColumn9Inter1Column);

  return mask;
}

}  // namespace cpu

}  // namespace starkware
