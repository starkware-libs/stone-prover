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

#ifndef STARKWARE_COMPOSITION_POLYNOMIAL_BREAKER_H_
#define STARKWARE_COMPOSITION_POLYNOMIAL_BREAKER_H_

#include <memory>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/fft/multiplicative_fft.h"
#include "starkware/algebra/polymorphic/field_element_span.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/fft_utils/fft_bases.h"

namespace starkware {

/*
  Handles "breaking" a polynomials f of degree 2^log_breaks * n, to 2^log_breaks polynomials of
  degree n s.t.
    f(x) = \sum_i x^i h_i(x^(2^log_breaks)).
*/
class PolynomialBreak {
 public:
  virtual ~PolynomialBreak() = default;

  /*
    Takes an evaluation of f(x) over a coset, returns the evaluations of h_i(x) on that coset.
    The coset is specified by the FftBases provided in the constructor. output is the storage
    in which the evaluations will be stored, should be the size of the coset. Returns a vector
    of 2^log_breaks subspans of output.
  */
  virtual std::vector<ConstFieldElementSpan> Break(
      const ConstFieldElementSpan& evaluation, const FieldElementSpan& output) const = 0;

  /*
    Given values of h_i(point) for all 2^log_breaks "broken" polynomials, computes f(point).
  */
  virtual FieldElement EvalFromSamples(
      const ConstFieldElementSpan& samples, const FieldElement& point) const = 0;
};

std::unique_ptr<PolynomialBreak> MakePolynomialBreak(const FftBases& bases, size_t log_breaks);

}  // namespace starkware

#endif  // STARKWARE_COMPOSITION_POLYNOMIAL_BREAKER_H_
