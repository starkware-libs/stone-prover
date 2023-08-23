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

#include "starkware/utils/task_manager.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
void EcOpBuiltinProverContext<FieldElementT>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  const Input dummy_input{kPrimeFieldEc0.k_points[0], kPrimeFieldEc0.k_points[1],
                          FieldElementT::FromUint(0)};

  for (uint64_t idx = 0; idx < n_instances_; ++idx) {
    const uint64_t mem_addr = begin_addr_ + 7 * idx;

    std::vector<FieldElementT> slopes = FieldElementT::UninitializedVector(height_ - 1);
    const auto& input_itr = inputs_.find(idx);
    const Input& input = input_itr != inputs_.end() ? input_itr->second : dummy_input;

    const auto& doubled_points = TwosPowersOfPoint(
        /*base=*/input.q,
        /*alpha=*/curve_config_.alpha,
        /*num_points=*/height_,
        /*slopes=*/std::make_optional(gsl::make_span(slopes)),
        /*allow_more_points=*/true);
    const auto& [doubled_points_x, doubled_points_y] =
        EcPointT::ToCoordinatesAndExpand(doubled_points);
    for (size_t i = 0; i < height_; ++i) {
      doubled_points_x_.SetCell(trace, idx * height_ + i, doubled_points_x[i]);
      doubled_points_y_.SetCell(trace, idx * height_ + i, doubled_points_y[i]);
      if (i < height_ - 1) {
        doubling_slope_.SetCell(trace, idx * height_ + i, slopes[i]);
      }
    }

    const auto& output =
        subset_sum_component_.WriteTrace(input.p, doubled_points, input.m, idx, trace);

    mem_p_x_.WriteTrace(idx, mem_addr, input.p.x, trace);
    mem_p_y_.WriteTrace(idx, mem_addr + 1, input.p.y, trace);
    mem_q_x_.WriteTrace(idx, mem_addr + 2, input.q.x, trace);
    mem_q_y_.WriteTrace(idx, mem_addr + 3, input.q.y, trace);
    mem_m_.WriteTrace(idx, mem_addr + 4, input.m, trace);
    mem_r_x_.WriteTrace(idx, mem_addr + 5, output.x, trace);
    mem_r_y_.WriteTrace(idx, mem_addr + 6, output.y, trace);
  }
}

template <typename FieldElementT>
auto EcOpBuiltinProverContext<FieldElementT>::ParsePrivateInput(const JsonValue& private_input)
    -> std::map<uint64_t, Input> {
  std::map<uint64_t, Input> res;

  for (size_t i = 0; i < private_input.ArrayLength(); ++i) {
    const auto& input = private_input[i];
    res.emplace(
        input["index"].AsUint64(),
        Input{/*p=*/EcPointT{input["p_x"].AsFieldElement<FieldElementT>(),
                             input["p_y"].AsFieldElement<FieldElementT>()},
              /*q=*/
              EcPointT{input["q_x"].AsFieldElement<FieldElementT>(),
                       input["q_y"].AsFieldElement<FieldElementT>()},
              /*m=*/input["m"].AsFieldElement<FieldElementT>()});
  }

  return res;
}

}  // namespace cpu
}  // namespace starkware
