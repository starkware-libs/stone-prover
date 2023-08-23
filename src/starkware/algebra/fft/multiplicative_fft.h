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

#ifndef STARKWARE_ALGEBRA_FFT_MULTIPLICATIVE_FFT_H_
#define STARKWARE_ALGEBRA_FFT_MULTIPLICATIVE_FFT_H_

#include <cstdint>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fft/multiplicative_group_ordering.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/fft_utils/fft_bases.h"
#include "starkware/math/math.h"

namespace starkware {

/*
  Computes FFT
  Input is in bit reversal order (R), output is in natural order (N).
  I.e it evaluates the polynomial with the coeffiecents
  BitReverse(src) on the domain offset*[w^0, w^1, ..., w^n-1]
  w is the src.size() root of unity.
*/
template <typename BasesT>
void MultiplicativeFft(
    const BasesT& bases, gsl::span<const typename BasesT::FieldElementT> src,
    gsl::span<typename BasesT::FieldElementT> dst);

/*
  Computes the Ifft.
  Input is in natural order (N), output is in bit reversal order (R).
  The output is the coefficents of the polynomial times the number of eval points (==src.size()).
  I.e. under our definition Ifft(fft(poly)) = deg(poly)*poly.
  w_inverse is the inverse of the root of unity used by the fft.
  offset_inverse is the inverse of the coset offset.
*/
template <typename BasesT>
void MultiplicativeIfft(
    const BasesT& bases, gsl::span<const typename BasesT::FieldElementT> src,
    gsl::span<typename BasesT::FieldElementT> dst, int n_layers = -1);

}  // namespace starkware

#include "starkware/algebra/fft/multiplicative_fft.inl"

#endif  // STARKWARE_ALGEBRA_FFT_MULTIPLICATIVE_FFT_H_
