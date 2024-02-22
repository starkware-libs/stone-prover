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

#include <utility>

#include "starkware/utils/bit_reversal.h"

namespace starkware {

template <typename FieldElementT>
const std::vector<FieldElementT> DegreeThreeExampleAir<FieldElementT>::kValues = {
    FieldElementT::FromUint(2), FieldElementT::FromUint(10)};
template <typename FieldElementT>
const FieldElementT DegreeThreeExampleAir<FieldElementT>::kCst = FieldElementT::FromUint(16);

template <typename FieldElementT>
void DegreeThreeExampleAir<FieldElementT>::BuildPeriodicColumns(
    const FieldElementT& gen, Builder* builder) const {
  builder->AddPeriodicColumn(
      PeriodicColumn<FieldElementT>(kValues, gen, FieldElementT::One(), trace_length_, 1),
      kAddThreePeriodicColumn);
}

template <typename FieldElementT>
Trace DegreeThreeExampleAir<FieldElementT>::GetTrace(
    const FieldElementT& witness, uint64_t trace_length, uint64_t res_claim_index) {
  ASSERT_RELEASE(IsPowerOfTwo(trace_length), "trace_length must be a power of 2.");
  ASSERT_RELEASE(
      res_claim_index < trace_length, "res_claim_index must be smaller than trace_length.");
  std::vector<std::vector<FieldElementT>> trace_values(1);
  trace_values[0].reserve(trace_length);
  FieldElementT x = witness;

  for (uint64_t i = 0; i < trace_length; ++i) {
    trace_values[0].push_back(x);
    x = kCst * Pow(x, 3) + kValues[i % 2];
  }

  return Trace(std::move(trace_values));
}

template <typename FieldElementT>
FieldElementT DegreeThreeExampleAir<FieldElementT>::PublicInputFromPrivateInput(
    const FieldElementT& witness, const uint64_t res_claim_index) {
  FieldElementT x = witness;
  for (uint64_t i = 0; i < res_claim_index; ++i) {
    x = kCst * Pow(x, 3) + kValues[i % 2];
  }
  return x;
}

}  // namespace starkware
