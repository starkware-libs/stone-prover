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

#ifndef STARKWARE_CRYPT_TOOLS_HASH_CONTEXT_PEDERSEN_HASH_CONTEXT_H_
#define STARKWARE_CRYPT_TOOLS_HASH_CONTEXT_PEDERSEN_HASH_CONTEXT_H_

#include <vector>

#include "starkware/crypt_tools/hash_context/hash_context.h"

#include "starkware/air/components/ec_subset_sum/ec_subset_sum.h"
#include "starkware/algebra/elliptic_curve/elliptic_curve_constants.h"
#include "starkware/algebra/fields/prime_field_element.h"

namespace starkware {

/*
  A struct representing the configuration for the Pedersen hash function. This way we can define
  the hash functionality as a standalone construct.
      n_element_bits - stores the number of bits in a single hash input.
      ec_subset_sum_height - stores the number of rows for each instance of subset sum.
      n_inputs - stores the number of hash inputs.
      shift_point - stores the initial curve point for the summation.
      points - stores the constant points for the hash.
*/
template <typename FieldElementT>
struct PedersenHashContext : public HashContext<FieldElementT> {
  uint64_t n_element_bits;
  uint64_t ec_subset_sum_height;
  size_t n_inputs;
  EcPoint<FieldElementT> shift_point;
  std::vector<EcPoint<FieldElementT>> points;

  PedersenHashContext(
      uint64_t n_element_bits, uint64_t ec_subset_sum_height, size_t n_inputs,
      const EcPoint<FieldElementT>& shift_point, const std::vector<EcPoint<FieldElementT>>& points)
      : n_element_bits(n_element_bits),
        ec_subset_sum_height(ec_subset_sum_height),
        n_inputs(n_inputs),
        shift_point(shift_point),
        points(points) {
    ASSERT_RELEASE(
        points.size() == n_element_bits * n_inputs,
        "points should be of length n_inputs * element_bits.");
  }

  /*
    Calculates the hash of the given inputs.
  */
  FieldElementT Hash(const gsl::span<const FieldElementT> hash_inputs) const {
    ASSERT_RELEASE(
        points.size() == n_element_bits * hash_inputs.size(),
        "The number of points is not equal to the number of bits in total in the hash input.");
    EcPoint<FieldElementT> cur_sum = shift_point;
    for (size_t i = 0; i < hash_inputs.size(); ++i) {
      const uint64_t offset = n_element_bits * i;
      auto points_span = gsl::make_span(points);

      cur_sum = EcSubsetSumComponent<FieldElementT>::Hash(
          cur_sum, points_span.subspan(offset, n_element_bits), hash_inputs[i]);
    }
    return cur_sum.x;
  }

  /*
    Defines the hash function used on pairs of field elements.
  */
  FieldElementT Hash(const FieldElementT& x, const FieldElementT& y) const override {
    return Hash(std::array<FieldElementT, 2>({x, y}));
  }
};

/*
  Returns the standard PedersenHashContext, using elliptic_curve_constants.h.
*/
const PedersenHashContext<PrimeFieldElement<252, 0>>& GetStandardPedersenHashContext();

}  // namespace starkware

#endif  // STARKWARE_CRYPT_TOOLS_HASH_CONTEXT_PEDERSEN_HASH_CONTEXT_H_
