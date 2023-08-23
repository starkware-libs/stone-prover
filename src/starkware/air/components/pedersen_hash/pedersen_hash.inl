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

namespace starkware {

template <typename FieldElementT>
FieldElementT PedersenHashComponent<FieldElementT>::WriteTrace(
    gsl::span<const FieldElementT> inputs, const uint64_t component_index,
    const gsl::span<const gsl::span<FieldElementT>> trace) const {
  ASSERT_RELEASE(inputs.size() == hash_ctx_.n_inputs, "Wrong number of inputs.");

  EcPoint<FieldElementT> cur_sum = hash_ctx_.shift_point;

  for (size_t i = 0; i < hash_ctx_.n_inputs; ++i) {
    const uint64_t points_idx = hash_ctx_.n_element_bits * i;
    const uint64_t subcomponent_index = component_index * hash_ctx_.n_inputs + i;
    cur_sum = ec_subset_sum_.WriteTrace(
        cur_sum, gsl::make_span(hash_ctx_.points).subspan(points_idx, hash_ctx_.n_element_bits),
        inputs[i], subcomponent_index, trace);
  }

  return cur_sum.x;
}

template <typename FieldElementT>
std::map<const std::string, std::vector<FieldElementT>>
PedersenHashFactory<FieldElementT>::ComputePeriodicColumnValues() const {
  const uint64_t n_points = hash_ctx_.n_inputs * hash_ctx_.n_element_bits;
  ASSERT_RELEASE(
      hash_ctx_.points.size() == n_points,
      "The number of constant points must be " + std::to_string(n_points));
  std::vector<FieldElementT> padded_points_x, padded_points_y;
  padded_points_x.reserve(hash_ctx_.n_inputs * hash_ctx_.ec_subset_sum_height);
  padded_points_y.reserve(hash_ctx_.n_inputs * hash_ctx_.ec_subset_sum_height);

  size_t idx = 0;
  for (size_t input = 0; input < hash_ctx_.n_inputs; input++) {
    size_t i = 0;
    for (; i < hash_ctx_.n_element_bits; ++i) {
      padded_points_x.push_back(hash_ctx_.points[idx].x);
      padded_points_y.push_back(hash_ctx_.points[idx].y);
      idx++;
    }
    for (; i < hash_ctx_.ec_subset_sum_height; ++i) {
      padded_points_x.push_back(hash_ctx_.points[idx - 1].x);
      padded_points_y.push_back(hash_ctx_.points[idx - 1].y);
    }
  }

  ASSERT_RELEASE(idx == hash_ctx_.points.size(), "Number of points mismatch.");
  return {{this->name_ + "/x", std::move(padded_points_x)},
          {this->name_ + "/y", std::move(padded_points_y)}};
}

}  // namespace starkware
