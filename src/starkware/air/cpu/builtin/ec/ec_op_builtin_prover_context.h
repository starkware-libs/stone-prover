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

#ifndef STARKWARE_AIR_CPU_BUILTIN_EC_EC_OP_BUILTIN_PROVER_CONTEXT_H_
#define STARKWARE_AIR_CPU_BUILTIN_EC_EC_OP_BUILTIN_PROVER_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/ec_subset_sum/ec_subset_sum.h"
#include "starkware/air/components/memory/memory.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/algebra/elliptic_curve/elliptic_curve.h"
#include "starkware/algebra/elliptic_curve/elliptic_curve_constants.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
class EcOpBuiltinProverContext {
 public:
  using EcPointT = EcPoint<FieldElementT>;
  using ValueType = typename FieldElementT::ValueType;
  using Config = typename EllipticCurveConstants<FieldElementT>::CurveConfig;

  struct Input {
    EcPointT p;
    EcPointT q;
    FieldElementT m;
  };

  static const Config& GetEcConfig() {
    static const gsl::owner<const Config*> kEcConfig =
        new Config{kPrimeFieldEc0.k_alpha, kPrimeFieldEc0.k_beta, kPrimeFieldEc0.k_order};
    return *kEcConfig;
  }

  EcOpBuiltinProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>* memory_pool, const uint64_t begin_addr, uint64_t height,
      uint64_t n_bits, uint64_t n_instances, const Config& curve_config,
      std::map<uint64_t, Input> inputs)
      : begin_addr_(begin_addr),
        n_instances_(n_instances),
        height_(height),
        n_bits_(n_bits),
        curve_config_(curve_config),
        inputs_(std::move(inputs)),
        mem_p_x_(memory_pool, name + "/p_x", ctx),
        mem_p_y_(memory_pool, name + "/p_y", ctx),
        mem_q_x_(memory_pool, name + "/q_x", ctx),
        mem_q_y_(memory_pool, name + "/q_y", ctx),
        mem_m_(memory_pool, name + "/m", ctx),
        mem_r_x_(memory_pool, name + "/r_x", ctx),
        mem_r_y_(memory_pool, name + "/r_y", ctx),
        doubled_points_x_(ctx.GetVirtualColumn(name + "/doubled_points/x")),
        doubled_points_y_(ctx.GetVirtualColumn(name + "/doubled_points/y")),
        doubling_slope_(ctx.GetVirtualColumn(name + "/doubling_slope")),
        subset_sum_component_(
            /*name=*/name + "/ec_subset_sum",
            /*ctx=*/ctx,
            /*component_height=*/height,
            /*n_points=*/n_bits,
            /*use_x_diff_inv=*/true,
            /*use_bit_unpacking=*/true) {}

  /*
    Writes the trace cells for the EC opreration builtin.
  */
  void WriteTrace(gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    Parses the private input for the signature builtin. private_input should be a list of
    objects of the form {
        "index": <index of instance>,
        "p_x": <x coordinate of the point p>,
        "p_y": <y coordinate of the point p>,
        "q_x": <x coordinate of the point q>,
        "q_y": <y coordinate of the point q>,
        "m": <the coefficient m>,
        "r_x": <x coordinate of the point r>,
        "r_y": <y coordinate of the point r>,
    }.
  */
  static std::map<uint64_t, Input> ParsePrivateInput(const JsonValue& private_input);

 private:
  const uint64_t begin_addr_;
  const uint64_t n_instances_;
  const uint64_t height_;
  const uint64_t n_bits_;
  const Config curve_config_;

  const std::map<uint64_t, Input> inputs_;

  const MemoryCellView<FieldElementT> mem_p_x_;
  const MemoryCellView<FieldElementT> mem_p_y_;
  const MemoryCellView<FieldElementT> mem_q_x_;
  const MemoryCellView<FieldElementT> mem_q_y_;
  const MemoryCellView<FieldElementT> mem_m_;
  const MemoryCellView<FieldElementT> mem_r_x_;
  const MemoryCellView<FieldElementT> mem_r_y_;
  const VirtualColumn doubled_points_x_;
  const VirtualColumn doubled_points_y_;
  const VirtualColumn doubling_slope_;

  const EcSubsetSumComponent<FieldElementT> subset_sum_component_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/builtin/ec/ec_op_builtin_prover_context.inl"

#endif  // STARKWARE_AIR_CPU_BUILTIN_EC_EC_OP_BUILTIN_PROVER_CONTEXT_H_
