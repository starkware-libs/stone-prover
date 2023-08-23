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

#ifndef STARKWARE_ALGEBRA_FIELD_TO_INT_H_
#define STARKWARE_ALGEBRA_FIELD_TO_INT_H_

#include "starkware/algebra/big_int.h"
#include "starkware/math/math.h"

namespace starkware {

/*
  Converts a field element to a uint64, asserting that it's small enough.
*/
template <typename FieldElementT>
uint64_t ToUint64(const FieldElementT& field_element) {
  const auto t_bigint = field_element.ToStandardForm();
  ASSERT_RELEASE(t_bigint < BigInt<2>({0, 1}), "Field element is out of range.");
  return t_bigint[0];
}

/*
  Converts a field element to a int64, asserting that it's in range.
  The field characteristic must be > 2**64.
*/
template <typename FieldElementT>
uint64_t ToInt64(const FieldElementT& field_element) {
  return ToUint64(field_element + FieldElementT::FromUint(Pow2(63))) - Pow2(63);
}

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_FIELD_TO_INT_H_
