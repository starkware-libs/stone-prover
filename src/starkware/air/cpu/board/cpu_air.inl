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

#include "starkware/air/boundary_constraints/boundary_periodic_column.h"
#include "starkware/air/cpu/builtin/bitwise/bitwise_builtin_prover_context.h"
#include "starkware/air/cpu/builtin/ec/ec_op_builtin_prover_context.h"
#include "starkware/air/cpu/builtin/hash/hash_builtin_prover_context.h"
#include "starkware/air/cpu/builtin/keccak/keccak_builtin_prover_context.h"
#include "starkware/air/cpu/builtin/modulo/add_mod_builtin_prover_context.h"
#include "starkware/air/cpu/builtin/modulo/mod_builtin_prover_context.h"
#include "starkware/air/cpu/builtin/modulo/mul_mod_builtin_prover_context.h"
#include "starkware/air/cpu/builtin/poseidon/poseidon_builtin_prover_context.h"
#include "starkware/air/cpu/builtin/range_check/range_check_builtin_prover_context.h"
#include "starkware/air/cpu/builtin/signature/signature_builtin_prover_context.h"
#include "starkware/utils/profiling.h"
#include "starkware/utils/task_manager.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT, int LayoutId>
constexpr uint64_t CpuAir<FieldElementT, LayoutId>::PedersenRatio() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return SafeDiv(
        this->dynamic_params_[CpuAir::kPedersenBuiltinRowRatioDynamicParam],
        CpuAir::kCpuComponentHeight * this->dynamic_params_[CpuAir::kCpuComponentStepDynamicParam]);
  } else {                                        // NOLINT: clang-tidy if constexpr bug.
    if constexpr (CpuAir::kHasPedersenBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
      return CpuAir::kPedersenBuiltinRatio;
    } else {  // NOLINT: clang-tidy if constexpr bug.
      return 0;
    }
  }
}

template <typename FieldElementT, int LayoutId>
constexpr uint64_t CpuAir<FieldElementT, LayoutId>::RangeCheckRatio() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return SafeDiv(
        this->dynamic_params_[CpuAir::kRangeCheckBuiltinRowRatioDynamicParam],
        CpuAir::kCpuComponentHeight * this->dynamic_params_[CpuAir::kCpuComponentStepDynamicParam]);
  } else {                                          // NOLINT: clang-tidy if constexpr bug.
    if constexpr (CpuAir::kHasRangeCheckBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
      return CpuAir::kRangeCheckBuiltinRatio;
    } else {  // NOLINT: clang-tidy if constexpr bug.
      return 0;
    }
  }
}

template <typename FieldElementT, int LayoutId>
constexpr uint64_t CpuAir<FieldElementT, LayoutId>::RangeCheck96NInstances() const {
  uint64_t row_ratio = 0, cpu_component_step = 0;
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    row_ratio = this->dynamic_params_[CpuAir::kRangeCheck96BuiltinRowRatioDynamicParam];
    cpu_component_step = this->dynamic_params_[CpuAir::kCpuComponentStepDynamicParam];
  } else {
    if constexpr (CpuAir::kHasRangeCheck96Builtin) {  // NOLINT: clang-tidy if constexpr bug.
      row_ratio = CpuAir::kRangeCheck96BuiltinRowRatio;
      cpu_component_step = CpuAir::kCpuComponentStep;
    } else {  // NOLINT: clang-tidy if constexpr bug.
      return 0;
    }
  }
  return SafeDiv(n_steps_ * cpu_component_step * CpuAir::kCpuComponentHeight, row_ratio);
}

template <typename FieldElementT, int LayoutId>
constexpr uint64_t CpuAir<FieldElementT, LayoutId>::EcdsaRatio() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return SafeDiv(
        this->dynamic_params_[CpuAir::kEcdsaBuiltinRowRatioDynamicParam],
        CpuAir::kCpuComponentHeight * this->dynamic_params_[CpuAir::kCpuComponentStepDynamicParam]);
  } else {                                     // NOLINT: clang-tidy if constexpr bug.
    if constexpr (CpuAir::kHasEcdsaBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
      return CpuAir::kEcdsaBuiltinRatio;
    } else {  // NOLINT: clang-tidy if constexpr bug.
      return 0;
    }
  }
}

template <typename FieldElementT, int LayoutId>
constexpr uint64_t CpuAir<FieldElementT, LayoutId>::BitwiseRatio() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return SafeDiv(
        this->dynamic_params_[CpuAir::kBitwiseRowRatioDynamicParam],
        CpuAir::kCpuComponentHeight * this->dynamic_params_[CpuAir::kCpuComponentStepDynamicParam]);
  } else {                                       // NOLINT: clang-tidy if constexpr bug.
    if constexpr (CpuAir::kHasBitwiseBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
      return CpuAir::kBitwiseRatio;
    } else {  // NOLINT: clang-tidy if constexpr bug.
      return 0;
    }
  }
}

template <typename FieldElementT, int LayoutId>
constexpr uint64_t CpuAir<FieldElementT, LayoutId>::EcOpRatio() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return SafeDiv(
        this->dynamic_params_[CpuAir::kEcOpBuiltinRowRatioDynamicParam],
        CpuAir::kCpuComponentHeight * this->dynamic_params_[CpuAir::kCpuComponentStepDynamicParam]);
  } else {                                    // NOLINT: clang-tidy if constexpr bug.
    if constexpr (CpuAir::kHasEcOpBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
      return CpuAir::kEcOpBuiltinRatio;
    } else {  // NOLINT: clang-tidy if constexpr bug.
      return 0;
    }
  }
}

template <typename FieldElementT, int LayoutId>
constexpr uint64_t CpuAir<FieldElementT, LayoutId>::KeccakRatio() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return SafeDiv(
        this->dynamic_params_[CpuAir::kKeccakRowRatioDynamicParam],
        CpuAir::kCpuComponentHeight * this->dynamic_params_[CpuAir::kCpuComponentStepDynamicParam]);
  } else {                                      // NOLINT: clang-tidy if constexpr bug.
    if constexpr (CpuAir::kHasKeccakBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
      return CpuAir::kKeccakRatio;
    } else {  // NOLINT: clang-tidy if constexpr bug.
      return 0;
    }
  }
}

template <typename FieldElementT, int LayoutId>
constexpr uint64_t CpuAir<FieldElementT, LayoutId>::PoseidonRatio() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return SafeDiv(
        this->dynamic_params_[CpuAir::kPoseidonRowRatioDynamicParam],
        CpuAir::kCpuComponentHeight * this->dynamic_params_[CpuAir::kCpuComponentStepDynamicParam]);
  } else {                                        // NOLINT: clang-tidy if constexpr bug.
    if constexpr (CpuAir::kHasPoseidonBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
      return CpuAir::kPoseidonRatio;
    } else {  // NOLINT: clang-tidy if constexpr bug.
      return 0;
    }
  }
}

template <typename FieldElementT, int LayoutId>
constexpr uint64_t CpuAir<FieldElementT, LayoutId>::AddModNInstances() const {
  uint64_t row_ratio = 0, cpu_component_step = 0;
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    row_ratio = this->dynamic_params_[CpuAir::kAddModRowRatioDynamicParam];
    cpu_component_step = this->dynamic_params_[CpuAir::kCpuComponentStepDynamicParam];
  } else {
    if constexpr (CpuAir::kHasAddModBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
      row_ratio = CpuAir::kAddModRowRatio;
      cpu_component_step = CpuAir::kCpuComponentStep;
    } else {  // NOLINT: clang-tidy if constexpr bug.
      return 0;
    }
  }
  return SafeDiv(n_steps_ * cpu_component_step * CpuAir::kCpuComponentHeight, row_ratio);
}

template <typename FieldElementT, int LayoutId>
constexpr uint64_t CpuAir<FieldElementT, LayoutId>::MulModNInstances() const {
  uint64_t row_ratio = 0, cpu_component_step = 0;
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    row_ratio = this->dynamic_params_[CpuAir::kMulModRowRatioDynamicParam];
    cpu_component_step = this->dynamic_params_[CpuAir::kCpuComponentStepDynamicParam];
  } else {
    if constexpr (CpuAir::kHasMulModBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
      row_ratio = CpuAir::kMulModRowRatio;
      cpu_component_step = CpuAir::kCpuComponentStep;
    } else {  // NOLINT: clang-tidy if constexpr bug.
      return 0;
    }
  }
  return SafeDiv(n_steps_ * cpu_component_step * CpuAir::kCpuComponentHeight, row_ratio);
}

template <typename FieldElementT, int LayoutId>
constexpr bool CpuAir<FieldElementT, LayoutId>::UsesPedersenBuiltin() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return this->dynamic_params_[CpuAir::kUsesPedersenBuiltinDynamicParam];
  } else {  // NOLINT: clang-tidy if constexpr bug.
    return CpuAir::kHasPedersenBuiltin;
  }
}

template <typename FieldElementT, int LayoutId>
constexpr bool CpuAir<FieldElementT, LayoutId>::UsesRangeCheckBuiltin() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return this->dynamic_params_[CpuAir::kUsesRangeCheckBuiltinDynamicParam];
  } else {  // NOLINT: clang-tidy if constexpr bug.
    return CpuAir::kHasRangeCheckBuiltin;
  }
}

template <typename FieldElementT, int LayoutId>
constexpr bool CpuAir<FieldElementT, LayoutId>::UsesRangeCheck96Builtin() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return this->dynamic_params_[CpuAir::kUsesRangeCheck96BuiltinDynamicParam];
  } else {  // NOLINT: clang-tidy if constexpr bug.
    return CpuAir::kHasRangeCheck96Builtin;
  }
}

template <typename FieldElementT, int LayoutId>
constexpr bool CpuAir<FieldElementT, LayoutId>::UsesEcdsaBuiltin() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return this->dynamic_params_[CpuAir::kUsesEcdsaBuiltinDynamicParam];
  } else {  // NOLINT: clang-tidy if constexpr bug.
    return CpuAir::kHasEcdsaBuiltin;
  }
}

template <typename FieldElementT, int LayoutId>
constexpr bool CpuAir<FieldElementT, LayoutId>::UsesBitwiseBuiltin() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return this->dynamic_params_[CpuAir::kUsesBitwiseBuiltinDynamicParam];
  } else {  // NOLINT: clang-tidy if constexpr bug.
    return CpuAir::kHasBitwiseBuiltin;
  }
}

template <typename FieldElementT, int LayoutId>
constexpr bool CpuAir<FieldElementT, LayoutId>::UsesEcOpBuiltin() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return this->dynamic_params_[CpuAir::kUsesEcOpBuiltinDynamicParam];
  } else {  // NOLINT: clang-tidy if constexpr bug.
    return CpuAir::kHasEcOpBuiltin;
  }
}

template <typename FieldElementT, int LayoutId>
constexpr bool CpuAir<FieldElementT, LayoutId>::UsesKeccakBuiltin() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return this->dynamic_params_[CpuAir::kUsesKeccakBuiltinDynamicParam];
  } else {  // NOLINT: clang-tidy if constexpr bug.
    return CpuAir::kHasKeccakBuiltin;
  }
}

template <typename FieldElementT, int LayoutId>
constexpr bool CpuAir<FieldElementT, LayoutId>::UsesPoseidonBuiltin() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return this->dynamic_params_[CpuAir::kUsesPoseidonBuiltinDynamicParam];
  } else {  // NOLINT: clang-tidy if constexpr bug.
    return CpuAir::kHasPoseidonBuiltin;
  }
}

template <typename FieldElementT, int LayoutId>
constexpr bool CpuAir<FieldElementT, LayoutId>::UsesAddModBuiltin() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return this->dynamic_params_[CpuAir::kUsesAddModBuiltinDynamicParam];
  } else {  // NOLINT: clang-tidy if constexpr bug.
    return CpuAir::kHasAddModBuiltin;
  }
}

template <typename FieldElementT, int LayoutId>
constexpr bool CpuAir<FieldElementT, LayoutId>::UsesMulModBuiltin() const {
  if constexpr (CpuAir::kIsDynamicAir) {  // NOLINT: clang-tidy if constexpr bug.
    return this->dynamic_params_[CpuAir::kUsesMulModBuiltinDynamicParam];
  } else {  // NOLINT: clang-tidy if constexpr bug.
    return CpuAir::kHasMulModBuiltin;
  }
}

template <typename FieldElementT, int LayoutId>
void CpuAir<FieldElementT, LayoutId>::BuildPeriodicColumns(
    const FieldElementT& gen, Builder* builder) const {
  // Pedersen builtin.
  if constexpr (CpuAir::kHasPedersenBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesPedersenBuiltin()) {
      for (const auto& [column_name, column_values] : hash_factory_.ComputePeriodicColumnValues()) {
        const auto& column_info = this->ctx_.GetPeriodicColumn(column_name);
        builder->AddPeriodicColumn(
            PeriodicColumn<FieldElementT>(
                column_values, gen, FieldElementT::One(), this->trace_length_,
                column_info.view.step),
            column_info.column);
      }
    } else {
      for (const auto& column_name : {"pedersen/points/x", "pedersen/points/y"}) {
        const auto& column_info = this->ctx_.GetPeriodicColumn(column_name);
        builder->AddPeriodicColumn(
            PeriodicColumn<FieldElementT>(
                CpuAir::kTrivialPeriodicColumnData, gen, FieldElementT::One(), this->trace_length_,
                1),
            column_info.column);
      }
    }
  }

  // Periodic columns for ecdsa constant column.
  if constexpr (CpuAir::kHasEcdsaBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesEcdsaBuiltin()) {
      const auto& [points_x, points_y] = EcPoint<FieldElementT>::ToCoordinatesAndExpand(
          TwosPowersOfPoint(
              this->ecdsa__sig_config_.generator_point, this->ecdsa__sig_config_.alpha,
              CpuAir::kEcdsaElementBits),
          CpuAir::kEcdsaElementHeight);

      const auto& column_info_x = this->ctx_.GetPeriodicColumn("ecdsa/generator_points/x");
      builder->AddPeriodicColumn(
          PeriodicColumn<FieldElementT>(
              points_x, gen, FieldElementT::One(), this->trace_length_, column_info_x.view.step),
          column_info_x.column);

      const auto& column_info_y = this->ctx_.GetPeriodicColumn("ecdsa/generator_points/y");
      builder->AddPeriodicColumn(
          PeriodicColumn<FieldElementT>(
              points_y, gen, FieldElementT::One(), this->trace_length_, column_info_y.view.step),
          column_info_y.column);
    } else {
      const auto& column_info_x = this->ctx_.GetPeriodicColumn("ecdsa/generator_points/x");
      builder->AddPeriodicColumn(
          PeriodicColumn<FieldElementT>(
              CpuAir::kTrivialPeriodicColumnData, gen, FieldElementT::One(), this->trace_length_,
              1),
          column_info_x.column);

      const auto& column_info_y = this->ctx_.GetPeriodicColumn("ecdsa/generator_points/y");
      builder->AddPeriodicColumn(
          PeriodicColumn<FieldElementT>(
              CpuAir::kTrivialPeriodicColumnData, gen, FieldElementT::One(), this->trace_length_,
              1),
          column_info_y.column);
    }
  }
}

template <typename FieldElementT, int LayoutId>
CpuAir<FieldElementT, LayoutId> CpuAir<FieldElementT, LayoutId>::WithInteractionElementsImpl(
    gsl::span<const FieldElementT> interaction_elms) const {
  CpuAir new_air(*this);
  unsigned int i = 0;
  new_air.memory__multi_column_perm__perm__interaction_elm_ = interaction_elms[i++];
  new_air.memory__multi_column_perm__hash_interaction_elm0_ = interaction_elms[i++];
  new_air.range_check16__perm__interaction_elm_ = interaction_elms[i++];
  new_air.memory__multi_column_perm__perm__public_memory_prod_ = new_air.GetPublicMemoryProd();
  if constexpr (CpuAir::kHasDilutedPool) {  // NOLINT: clang-tidy if constexpr bug.
    new_air.diluted_check__permutation__interaction_elm_ = interaction_elms[i++];
    new_air.diluted_check__interaction_z_ = interaction_elms[i++];
    new_air.diluted_check__interaction_alpha_ = interaction_elms[i++];
    new_air.diluted_check__final_cum_val_ =
        DilutedCheckComponentProverContext1<FieldElementT>::ExpectedFinalCumulativeValue(
            CpuAir::kDilutedSpacing, CpuAir::kDilutedNBits, new_air.diluted_check__interaction_z_,
            new_air.diluted_check__interaction_alpha_);
  }
  if constexpr (CpuAir::kHasAddModBuiltin) {
    new_air.add_mod__interaction_elm_ = interaction_elms[i++];
  }
  if constexpr (CpuAir::kHasMulModBuiltin) {
    new_air.mul_mod__interaction_elm_ = interaction_elms[i++];
  }
  ASSERT_RELEASE(interaction_elms.size() == i, "Interaction element vector is of wrong size.");
  return new_air;
}

template <typename FieldElementT, int LayoutId>
std::pair<CpuAirProverContext1<FieldElementT>, Trace> CpuAir<FieldElementT, LayoutId>::GetTrace(
    gsl::span<const TraceEntry<FieldElementT>> cpu_trace,
    MaybeOwnedPtr<CpuMemory<FieldElementT>> memory, const JsonValue& private_input) const {
  ASSERT_RELEASE(cpu_trace.size() == n_steps_, "Wrong number of trace entries.");

  ProfilingBlock init_trace_block("Init trace memory");
  std::vector<std::vector<FieldElementT>> trace(
      this->GetInteractionParams()->n_columns_first,
      std::vector<FieldElementT>(this->trace_length_, FieldElementT::Zero()));
  std::vector<gsl::span<FieldElementT>> trace_spans(trace.begin(), trace.end());
  init_trace_block.CloseBlock();

  MemoryCell<FieldElementT> memory_pool("mem_pool", ctx_, this->trace_length_);
  RangeCheckCell<FieldElementT> rc16_pool("range_check16_pool", ctx_, this->trace_length_);

  {
    ProfilingBlock cpu_component_block("CpuComponent::WriteTrace");
    TaskManager::GetInstance().ParallelFor(cpu_trace.size(), [&](const TaskInfo& task_info) {
      const size_t idx = task_info.start_idx;
      cpu_component_.WriteTrace(
          idx, cpu_trace[idx], *memory, &memory_pool, &rc16_pool, trace_spans);
    });
  }

  // Write public memory in trace.
  WritePublicMemory(&memory_pool, trace_spans);

  // Pedersen builtin.
  if constexpr (CpuAir::kHasPedersenBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesPedersenBuiltin()) {
      ProfilingBlock pedersen_builtin_block("Pedersen builtin");
      HashBuiltinProverContext<FieldElementT>(
          "pedersen", ctx_, hash_factory_, &memory_pool, this->pedersen_begin_addr_,
          SafeDiv(n_steps_, PedersenRatio()), CpuAir::kPedersenBuiltinRepetitions,
          HashBuiltinProverContext<FieldElementT>::ParsePrivateInput(private_input["pedersen"]))
          .WriteTrace(trace_spans);
    }
  }

  // Range check builtin.
  std::optional<RangeCheckBuiltinProverContext<FieldElementT>> rc_prover;
  if constexpr (CpuAir::kHasRangeCheckBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesRangeCheckBuiltin()) {
      rc_prover.emplace(
          "range_check_builtin", ctx_, &memory_pool, &rc16_pool, this->range_check_begin_addr_,
          SafeDiv(n_steps_, RangeCheckRatio()), CpuAir::kRangeCheckNParts, CpuAir::kOffsetBits,
          RangeCheckBuiltinProverContext<FieldElementT>::ParsePrivateInput(
              private_input["range_check"]));

      ProfilingBlock range_check_builtin_block("Range check builtin");
      rc_prover->WriteTrace(trace_spans);
    }
  }

  // Range check 96 builtin.
  std::optional<RangeCheckBuiltinProverContext<FieldElementT>> rc96_prover;
  if constexpr (CpuAir::kHasRangeCheck96Builtin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesRangeCheck96Builtin()) {
      rc96_prover.emplace(
          "range_check96_builtin", ctx_, &memory_pool, &rc16_pool, this->range_check96_begin_addr_,
          RangeCheck96NInstances(), CpuAir::kRangeCheck96NParts, CpuAir::kOffsetBits,
          RangeCheckBuiltinProverContext<FieldElementT>::ParsePrivateInput(
              private_input["range_check96"]));

      ProfilingBlock range_check_builtin_block("Range check 96 builtin");
      rc96_prover->WriteTrace(trace_spans);
    }
  }

  // ECDSA builtin.
  if constexpr (CpuAir::kHasEcdsaBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesEcdsaBuiltin()) {
      ProfilingBlock ecdsa_builtin_block("ECDSA builtin");
      SignatureBuiltinProverContext<FieldElementT>(
          "ecdsa", ctx_, &memory_pool, this->ecdsa_begin_addr_, CpuAir::kEcdsaElementHeight,
          CpuAir::kEcdsaElementBits, SafeDiv(n_steps_, EcdsaRatio()),
          CpuAir::kEcdsaBuiltinRepetitions, this->ecdsa__sig_config_,
          SignatureBuiltinProverContext<FieldElementT>::ParsePrivateInput(
              private_input["ecdsa"], this->ecdsa__sig_config_))
          .WriteTrace(trace_spans);
    }
  }

  std::optional<diluted_check_cell::DilutedCheckCell<FieldElementT>> diluted_pool;
  if constexpr (CpuAir::kHasDilutedPool) {  // NOLINT: clang-tidy if constexpr bug.
    diluted_pool.emplace(
        "diluted_pool", ctx_, this->trace_length_, CpuAir::kDilutedSpacing, CpuAir::kDilutedNBits);
  }

  // Bitwise builtin.
  if constexpr (CpuAir::kHasBitwiseBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesBitwiseBuiltin()) {
      ProfilingBlock bitwise_builtin_block("Bitwise builtin");
      BitwiseBuiltinProverContext<FieldElementT>(
          "bitwise", ctx_, &memory_pool, &*diluted_pool, this->bitwise_begin_addr_,
          SafeDiv(n_steps_, BitwiseRatio()), CpuAir::kDilutedSpacing, CpuAir::kDilutedNBits,
          CpuAir::kBitwiseTotalNBits,
          BitwiseBuiltinProverContext<FieldElementT>::ParsePrivateInput(private_input["bitwise"]))
          .WriteTrace(trace_spans);
    }
  }

  // EcOp builtin.
  if constexpr (CpuAir::kHasEcOpBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesEcOpBuiltin()) {
      ProfilingBlock ec_op_builtin_block("EC operation builtin");
      EcOpBuiltinProverContext<FieldElementT>(
          /*name=*/"ec_op",
          /*ctx=*/ctx_,
          /*memory_pool=*/&memory_pool,
          /*begin_addr=*/this->ec_op_begin_addr_,
          /*height=*/CpuAir::kEcOpScalarHeight,
          /*n_bits=*/CpuAir::kEcOpNBits, SafeDiv(n_steps_, EcOpRatio()),
          /*curve_config=*/this->ec_op__curve_config_,
          /*inputs=*/
          EcOpBuiltinProverContext<FieldElementT>::ParsePrivateInput(private_input["ec_op"]))
          .WriteTrace(trace_spans);
    }
  }

  // Keccak builtin.
  if constexpr (CpuAir::kHasKeccakBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesKeccakBuiltin()) {
      ProfilingBlock keccak_builtin_block("Keccak builtin");
      KeccakBuiltinProverContext<FieldElementT>(
          "keccak", ctx_, &memory_pool, &*diluted_pool, this->keccak_begin_addr_,
          SafeDiv(n_steps_, KeccakRatio() * CpuAir::kDilutedNBits), CpuAir::kDilutedSpacing,
          CpuAir::kDilutedNBits,
          KeccakBuiltinProverContext<FieldElementT>::ParsePrivateInput(private_input["keccak"]))
          .WriteTrace(trace_spans);
    }
  }

  // Poseidon builtin.
  if constexpr (CpuAir::kHasPoseidonBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesPoseidonBuiltin()) {
      ProfilingBlock poseidon_builtin_block("Poseidon builtin");
      const ConstSpanAdapter<FieldElementT> mds_spans{
          gsl::span<const std::array<FieldElementT, CpuAir::kPoseidonM>>{CpuAir::kPoseidonMds}};
      const ConstSpanAdapter<FieldElementT> ark_spans{
          gsl::span<const std::array<FieldElementT, CpuAir::kPoseidonM>>{CpuAir::kPoseidonArk}};
      PoseidonBuiltinProverContext<FieldElementT, CpuAir::kPoseidonM>(
          "poseidon", ctx_, &memory_pool, this->poseidon_begin_addr_,
          SafeDiv(n_steps_, PoseidonRatio()),
          PoseidonBuiltinProverContext<FieldElementT, CpuAir::kPoseidonM>::ParsePrivateInput(
              private_input["poseidon"]),
          CpuAir::kPoseidonRoundsFull, CpuAir::kPoseidonRoundsPartial,
          CpuAir::kPoseidonPartialRoundsPartition, mds_spans, ark_spans)
          .WriteTrace(trace_spans);
    }
  }

  // AddMod builtin.
  if constexpr (CpuAir::kHasAddModBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesAddModBuiltin()) {
      ProfilingBlock add_mod_builtin_block("AddMod builtin");
      AddModBuiltinProverContext<FieldElementT, CpuAir::kAddModNWords>(
          /*name=*/"add_mod",
          /*ctx=*/ctx_,
          /*memory_pool=*/&memory_pool,
          /*begin_addr=*/this->add_mod_begin_addr_,
          /*n_instances=*/AddModNInstances(),
          /*batch_size=*/CpuAir::kAddModBatchSize,
          /*word_bit_len=*/CpuAir::kAddModWordBitLen,
          /*inputs=*/
          ModBuiltinProverContext<FieldElementT, CpuAir::kAddModNWords>::ParsePrivateInput(
              private_input["add_mod"], CpuAir::kAddModBatchSize))
          .WriteTrace(trace_spans);
    }
  }

  // MulMod builtin.
  if constexpr (CpuAir::kHasMulModBuiltin) {  // NOLINT: clang-tidy if constexpr bug.
    if (UsesMulModBuiltin()) {
      ProfilingBlock mul_mod_builtin_block("MulMod builtin");
      MulModBuiltinProverContext<FieldElementT, CpuAir::kMulModNWords>(
          /*name=*/"mul_mod",
          /*ctx=*/ctx_,
          /*memory_pool=*/&memory_pool,
          /*rc_pool=*/&rc16_pool,
          /*begin_addr=*/this->mul_mod_begin_addr_,
          /*n_instances=*/MulModNInstances(),
          /*batch_size=*/CpuAir::kMulModBatchSize,
          /*word_bit_len=*/CpuAir::kMulModWordBitLen,
          /*bits_per_part=*/16,
          /*inputs=*/
          ModBuiltinProverContext<FieldElementT, CpuAir::kMulModNWords>::ParsePrivateInput(
              private_input["mul_mod"], CpuAir::kMulModBatchSize))
          .WriteTrace(trace_spans);
    }
  }

  // Finalize.
  rc16_pool.Finalize(rc_min_, rc_max_, trace_spans);
  if (rc_prover.has_value()) {
    // rc_prover writes to memory, so this must be called before the memory finalization.
    rc_prover->Finalize(trace_spans);
  }
  if (rc96_prover.has_value()) {
    // rc96_prover writes to memory, so this must be called before the memory finalization.
    rc96_prover->Finalize(trace_spans);
  }
  if constexpr (CpuAir::kHasDilutedPool) {  // NOLINT: clang-tidy if constexpr bug.
    diluted_pool->Finalize(trace_spans);
  }
  memory_pool.Finalize(trace_spans, this->disable_asserts_in_memory_);

  MemoryComponentProverContext1<FieldElementT> memory_prover_context1 =
      MemoryComponentProverContext<FieldElementT>("memory", ctx_, std::move(memory_pool))
          .WriteTrace(trace_spans, this->disable_asserts_in_memory_);

  PermRangeCheckComponentProverContext1<FieldElementT> perm_range_check_prover_context1 =
      PermRangeCheckComponentProverContext0<FieldElementT>(
          "range_check16", ctx_, std::move(rc16_pool))
          .WriteTrace(trace_spans);

  std::optional<DilutedCheckComponentProverContext1<FieldElementT>> diluted_check_prover_context1;
  if constexpr (CpuAir::kHasDilutedPool) {  // NOLINT: clang-tidy if constexpr bug.
    diluted_check_prover_context1.emplace(DilutedCheckComponentProverContext0<FieldElementT>(
                                              "diluted_check", CpuAir::kDilutedSpacing,
                                              CpuAir::kDilutedNBits, ctx_,
                                              ConsumeOptional(std::move(diluted_pool)))
                                              .WriteTrace(trace_spans));
  }

  return {
      CpuAirProverContext1<FieldElementT>{
          /*memory_prover_context1=*/std::move(memory_prover_context1),
          /*perm_range_check_prover_context1=*/std::move(perm_range_check_prover_context1),
          /*diluted_check_prover_context1=*/std::move(diluted_check_prover_context1),
      },
      Trace(std::move(trace))};
}

template <typename FieldElementT, int LayoutId>
Trace CpuAir<FieldElementT, LayoutId>::GetInteractionTrace(
    CpuAirProverContext1<FieldElementT>&& cpu_air_prover_context1) const {
  std::vector<std::vector<FieldElementT>> trace(
      this->GetInteractionParams()->n_columns_second,
      std::vector<FieldElementT>(this->trace_length_, FieldElementT::Zero()));

  const std::vector<FieldElementT> interaction_elms_vec = {
      this->memory__multi_column_perm__perm__interaction_elm_,
      this->memory__multi_column_perm__hash_interaction_elm0_};

  std::move(cpu_air_prover_context1.memory_prover_context1)
      .WriteTrace(
          interaction_elms_vec, SpanAdapter(trace),
          this->memory__multi_column_perm__perm__public_memory_prod_);
  std::move(cpu_air_prover_context1.perm_range_check_prover_context1)
      .WriteTrace(this->range_check16__perm__interaction_elm_, SpanAdapter(trace));

  if constexpr (CpuAir::kHasDilutedPool) {  // NOLINT: clang-tidy if constexpr bug.
    ConsumeOptional(std::move(cpu_air_prover_context1.diluted_check_prover_context1))
        .WriteTrace(
            this->diluted_check__permutation__interaction_elm_, this->diluted_check__interaction_z_,
            this->diluted_check__interaction_alpha_, SpanAdapter(trace));
  }

  return Trace(std::move(trace));
}

template <typename FieldElementT, int LayoutId>
void CpuAir<FieldElementT, LayoutId>::WritePublicMemory(
    MemoryCell<FieldElementT>* memory_pool, gsl::span<const gsl::span<FieldElementT>> trace) const {
  const MemoryCellView<FieldElementT> public_memory(memory_pool, "orig/public_memory", ctx_);
  ASSERT_RELEASE(public_memory_.size() <= public_memory.Size(), "public_memory_ is too large.");

  // Fill the trace with the public memory values.
  for (size_t i = 0; i < public_memory_.size(); ++i) {
    public_memory.WriteTrace(i, public_memory_[i].address, public_memory_[i].value, trace, true);
  }

  // Fill the rest of the public_memory virtual column cells with the first address-value pair of
  // the public memory.
  ASSERT_RELEASE(!public_memory_.empty(), "public_memory_ in cpu_air is empty.");
  const auto pad_address = public_memory_[0].address;
  const auto& pad_value = public_memory_[0].value;
  for (size_t i = public_memory_.size(); i < public_memory.Size(); ++i) {
    public_memory.WriteTrace(i, pad_address, pad_value, trace, true);
  }
}

template <typename FieldElementT, int LayoutId>
FieldElementT CpuAir<FieldElementT, LayoutId>::GetPublicMemoryProd() const {
  const FieldElementT& z = this->memory__multi_column_perm__perm__interaction_elm_;
  const FieldElementT& alpha = this->memory__multi_column_perm__hash_interaction_elm0_;
  const auto& public_memory_view = ctx_.GetVirtualColumn("orig/public_memory/addr").view;

  // The numerator of the public memory product is of the following form:
  // (z - (0 + alpha * 0))^(public_memory_length) = z^(public_memory_length).
  const FieldElementT numerator = Pow(z, public_memory_view.Size(this->trace_length_));

  // Compute the denominator of the public memory product. In each iteration, the cumulative
  // denominator is multiplied by the shifted hash of the next address-value pair of the public
  // memory.
  auto denominator = FieldElementT::One();
  for (size_t i = 0; i < public_memory_.size(); i++) {
    const auto val =
        z - (FieldElementT::FromUint(public_memory_[i].address) + alpha * public_memory_[i].value);
    denominator *= val;
  }
  // Compute the rest of the denominator using the padding (see GetTrace() for more detalis).
  const FieldElementT& pad_address = FieldElementT::FromUint(public_memory_[0].address);
  const FieldElementT& pad_value = public_memory_[0].value;
  const FieldElementT& val = z - (pad_address + alpha * pad_value);
  const FieldElementT& denominator_pad =
      Pow(val, public_memory_view.Size(this->trace_length_) - public_memory_.size());
  denominator *= denominator_pad;

  return numerator / denominator;
}

}  // namespace cpu
}  // namespace starkware
