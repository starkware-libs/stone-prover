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

#include "starkware/crypt_tools/hash_context/pedersen_hash_context.h"

namespace starkware {

const PedersenHashContext<PrimeFieldElement<252, 0>>& GetStandardPedersenHashContext() {
  using FieldElementT = PrimeFieldElement<252, 0>;

  static constexpr size_t kNHashInputs = 2;
  static constexpr size_t kElementBitsHash = 252;
  static constexpr size_t kEcSubsetSumHeight = 256;
  ASSERT_RELEASE(
      kPrimeFieldEc0.k_points.size() >= 2 + kNHashInputs * kElementBitsHash,
      "kPrimeFieldEc0.k_points must be large enough.");

  static const gsl::owner<const PedersenHashContext<FieldElementT>*> kHashContext =
      new PedersenHashContext<FieldElementT>{
          kElementBitsHash,
          /*ec_subset_sum_height=*/kEcSubsetSumHeight,
          /*n_inputs=*/kNHashInputs,
          /*shift_point=*/kPrimeFieldEc0.k_points[0],
          /*points=*/
          {kPrimeFieldEc0.k_points.begin() + 2,
           kPrimeFieldEc0.k_points.begin() + 2 + kNHashInputs * kElementBitsHash}};
  return *kHashContext;
}

}  // namespace starkware
