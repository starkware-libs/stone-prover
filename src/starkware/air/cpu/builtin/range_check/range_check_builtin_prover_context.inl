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
namespace cpu {

template <typename FieldElementT>
void RangeCheckBuiltinProverContext<FieldElementT>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  // shift_bits_ is guaranteed to be less than 64.
  const uint64_t mask = (uint64_t(1) << shift_bits_) - 1;

  for (auto input_itr = inputs_.cbegin(); input_itr != inputs_.cend(); ++input_itr) {
    ValueType input = input_itr->second;
    // Split value to n_parts;
    for (size_t part = 0; part < n_parts_; ++part) {
      rc_value_.WriteTrace(
          input_itr->first * n_parts_ + n_parts_ - part - 1, input[0] & mask, trace);
      input >>= shift_bits_;
    }
    ASSERT_RELEASE(
        input == ValueType::Zero(),
        "Too large value encountered in the range check builtin private input.");
  }
}

template <typename FieldElementT>
void RangeCheckBuiltinProverContext<FieldElementT>::Finalize(
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  for (size_t i = 0; i < n_instances_; ++i) {
    auto value = FieldElementT::ValueType::Zero();
    for (size_t part = 0; part < n_parts_; part++) {
      value <<= shift_bits_;
      value[0] |= rc_value_.Get(i * n_parts_ + part);
    }
    mem_input_.WriteTrace(i, begin_addr_ + i, FieldElementT::FromBigInt(value), trace);
  }
}

template <typename FieldElementT>
auto RangeCheckBuiltinProverContext<FieldElementT>::ParsePrivateInput(
    const JsonValue& private_input) -> std::map<uint64_t, ValueType> {
  std::map<uint64_t, ValueType> res;

  for (size_t i = 0; i < private_input.ArrayLength(); ++i) {
    const auto& input = private_input[i];
    res.emplace(input["index"].AsUint64(), ValueType::FromString(input["value"].AsString()));
  }

  return res;
}

}  // namespace cpu
}  // namespace starkware
