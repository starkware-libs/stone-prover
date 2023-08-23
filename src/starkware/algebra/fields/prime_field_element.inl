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

#include "starkware/algebra/big_int.h"

namespace starkware {

#ifndef __EMSCRIPTEN__
/*
  This function is implemented in assembly, in prime_field_elements.S .
*/
extern "C" BigInt<4> UnreducedMontMulPrime0(const BigInt<4>& x, const BigInt<4>& y);

/*
  This function can be used for both reverse order and natural order Ffts
  by passing the appropriate twiddle_shift and twiddle_mask.
  The butterfly that operates on src[idx] and src[idx+distance]
  uses the twiddle factor twiddle_factors[(idx >> twiddle_shift) & twiddle_mask].

  The required configurations are as follows:
    Natural order case:
      twiddle_shift = 0.
      aligned_twiddle_mask = (distance_in_bytes-1).
    Reverse order case:
      twiddle_shift = 1 + log2(distance)
      aligned_twiddle_mask = ~(sizeof(PrimeFieldElement<252, 0>).

  This function is implemented in assembly, in prime_field_elements.S .
*/
extern "C" void Prime0FftLoop(
    const PrimeFieldElement<252, 0>* src_plus_distance, const PrimeFieldElement<252, 0>* src_end,
    uint64_t src_to_dst, uint64_t distance_in_bytes, const PrimeFieldElement<252, 0>* twiddle_array,
    uint64_t twiddle_shift, uint64_t aligned_twiddle_mask);

/*
  Specific implementation of UnreducedMontgomeryMul for PrimeFieldElement<252,0> for performance
  reasons.
*/
template <>
template <>
inline BigInt<4> PrimeFieldElement<252, 0>::UnreducedMontgomeryMul<false>(
    const BigInt<4>& x, const BigInt<4>& y) {
  return UnreducedMontMulPrime0(x, y);
}

#endif

// Surpress instantiations outside of prime_field_element.cc.
// This must appear after all template specializations.
extern template class PrimeFieldElement<252, 0>;
extern template class PrimeFieldElement<254, 1>;
extern template class PrimeFieldElement<254, 2>;
extern template class PrimeFieldElement<252, 3>;
extern template class PrimeFieldElement<255, 4>;
extern template class PrimeFieldElement<124, 5>;

}  // namespace starkware
