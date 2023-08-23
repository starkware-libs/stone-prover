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

#ifndef STARKWARE_ALGEBRA_POLYNOMIALS_H_
#define STARKWARE_ALGEBRA_POLYNOMIALS_H_

#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"

namespace starkware {

/*
  Returns the interpolant polynomial p(x) of degree less than domain.size(), such that for every 'i'
  it holds that p(domain[i]) = values[i].
  Implemented using naive Lagrange interpolation. This function is not efficient, its
  complexity is O(n^3). Use only for testing or small inputs.
*/
template <typename FieldElementT>
std::vector<FieldElementT> LagrangeInterpolation(
    gsl::span<const FieldElementT> domain, gsl::span<const FieldElementT> values);

/*
  Returns the coefficients of a random polynomial of degree exactly deg.
  The first coefficient is the free coefficient, so that:
    f(x) = coefs[0] + x * coefs[1] + x^2 * coefs[2] + ...
*/
template <typename FieldElementT>
std::vector<FieldElementT> GetRandomPolynomial(size_t deg, Prng* prng);

/*
  Evaluates a polynomials with the given coefficients at a point x.
*/
template <typename FieldElementT>
FieldElementT HornerEval(const FieldElementT& x, const std::vector<FieldElementT>& coefs);

template <typename FieldElementT>
FieldElementT HornerEval(const FieldElementT& x, gsl::span<const FieldElementT> coefs);

/*
  Same as HornerEval(), for many points.
  If stride > 1, treats coefs as a vector of stride interleaved polynomials:
    coef 0 of poly 0, coef 0 of poly 1, ..., coef 0 of poly (stride-1),
    coef 1 of poly 0, coef 1 of poly 1, ..., coef 1 of poly (stride-1),
    ...
  The output will have the form:
    poly 0 at points[0], poly 1 at points[0], ..., poly (stride-1) at points[0],
    poly 0 at points[1], poly 1 at points[1], ..., poly (stride-1) at points[1],
    ...
*/
template <typename FieldElementT>
void BatchHornerEval(
    gsl::span<const FieldElementT> points, gsl::span<const FieldElementT> coefs,
    gsl::span<FieldElementT> outputs, size_t stride = 1);

/*
  Same as BatchHornerEval() but takes advantage of multithreading.

  It is less efficient for small polynomials, so it accepts a min_chunk_size
  to control how fine grained the parallelization is.
*/
template <typename FieldElementT>
void ParallelBatchHornerEval(
    gsl::span<const FieldElementT> points, gsl::span<const FieldElementT> coefs,
    gsl::span<FieldElementT> outputs, size_t stride = 1, size_t min_chunk_size = Pow2(13));

/*
  Same as ParallelBatchHornerEval() with the following optimization:
  If the set of points is {x, -x, y, -y, ...} (not necessarily in this order), then
  the polynomial is split into two polynomials f(x) = g(x^2) + xh(x^2) and
  the values of g(x^2) and h(x^2) are reused to compute f(x) and f(-x).
  Similar optimization is implemented when the points are in larger cosets (e.g., x, ix, -x, -ix)
  of size which is a power of two.
*/
template <typename FieldElementT>
void OptimizedBatchHornerEval(
    gsl::span<const FieldElementT> points, gsl::span<const FieldElementT> coefs,
    gsl::span<FieldElementT> outputs, size_t stride = 1);

/*
  Same as HornerEval except that coefs are given in bit reversed order (and particularly it is
  of size which is a power of two).
*/
template <typename FieldElementT>
FieldElementT HornerEvalBitReversed(
    const FieldElementT& x, const std::vector<FieldElementT>& coefs);

template <typename FieldElementT>
FieldElementT HornerEvalBitReversed(const FieldElementT& x, gsl::span<const FieldElementT> coefs);

/*
  Same as HornerEvalBitReversed(), for many points.
*/
template <typename FieldElementT>
void BatchHornerEvalBitReversed(
    gsl::span<const FieldElementT> points, gsl::span<const FieldElementT> coefs,
    gsl::span<FieldElementT> outputs);

}  // namespace starkware

#include "starkware/algebra/polynomials.inl"

#endif  // STARKWARE_ALGEBRA_POLYNOMIALS_H_
