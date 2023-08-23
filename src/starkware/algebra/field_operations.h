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

#ifndef STARKWARE_ALGEBRA_FIELD_OPERATIONS_H_
#define STARKWARE_ALGEBRA_FIELD_OPERATIONS_H_

#include <cstdint>
#include <functional>
#include <tuple>
#include <vector>

#include "glog/logging.h"

#include "starkware/error_handling/error_handling.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"

namespace starkware {

using std::size_t;
using std::uint64_t;

// --- Basic field-agnostic operations ---

/*
  Returns the product of multiplier and multiplicand.
  Note that this function is better then field multiplication only for a very small multiplier.
*/
template <typename FieldElementT>
FieldElementT Times(uint8_t multiplier, const FieldElementT& multiplicand) {
  if (multiplier == 0) {
    return FieldElementT::Zero();
  }
  FieldElementT product = multiplicand;
  for (int i = 0; i < multiplier - 1; ++i) {
    product += multiplicand;
  }
  return product;
}

/*
  Returns the power of a field element.
  Note that this function doesn't support negative exponents.
*/
template <typename FieldElementT>
FieldElementT Pow(const FieldElementT& base, uint64_t exp) {
  FieldElementT power = base;
  FieldElementT res = FieldElementT::One();
  while (exp != 0) {
    if ((exp & 1) == 1) {
      res *= power;
    }

    power *= power;
    exp >>= 1;
  }

  return res;
}

/*
  Returns the power of a field element.
  Note that this function doesn't support negative exponents.
*/
template <typename FieldElementT>
FieldElementT Pow(const FieldElementT& base, const std::vector<bool>& exponent_bits) {
  return GenericPow(
      base, exponent_bits, FieldElementT::One(),
      [](const FieldElementT& multiplier, FieldElementT* dst) { *dst *= multiplier; });
}

/*
  Returns true if value is a square of another field element.
  This function only works for prime fields (F_p where p is a prime and not a prime power).
*/
template <typename FieldElementT>
bool IsSquare(const FieldElementT& value);

/*
  Computes the square root of a field element. This function only works for prime fields (F_p where
  p is a prime and not a prime power).
  If value is not a square, an assertion is raised.
*/
template <typename FieldElementT>
FieldElementT Sqrt(const FieldElementT& value);

// --- Getters ---

/*
  Returns a generator of a subgroup with size n.
*/
template <typename FieldElementT>
FieldElementT GetSubGroupGenerator(uint64_t n) {
  using IntType = decltype(FieldElementT::FieldSize());
  IntType quotient, remainder;
  std::tie(quotient, remainder) =
      IntType::Div(IntType::Sub(FieldElementT::FieldSize(), IntType::One()).first, IntType(n));

  ASSERT_RELEASE(remainder == IntType::Zero(), "No subgroup of required size exists");
  return Pow(FieldElementT::Generator(), quotient.ToBoolVector());
}

template <typename FieldElementT>
FieldElementT RandomNonZeroElement(Prng* prng) {
  FieldElementT x = FieldElementT::RandomElement(prng);
  while (x == FieldElementT::Zero()) {
    x = FieldElementT::RandomElement(prng);
  }
  return x;
}

// --- Batch operations ---

/*
  Given a field element g and len return the vector [g, g^2, g^4, ..., g^(2*(len-1)].
*/
template <typename FieldElementT>
std::vector<FieldElementT> GetSquares(const FieldElementT& g, size_t len) {
  std::vector<FieldElementT> elements;
  FieldElementT generator = g;
  elements.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    elements.push_back(generator);
    generator *= generator;
  }
  return elements;
}

/*
  Returns the queried powers of a field element.
  Note that this function doesn't support negative exponents.
  More precisely, given as input base=x and exp={n1,n2,n3}, the output is computed to be {x^n1,
  x^n2, x^n3}.
*/
template <typename FieldElementT>
void BatchPow(
    const FieldElementT& base, gsl::span<const uint64_t> exponents,
    gsl::span<FieldElementT> output) {
  ASSERT_RELEASE(exponents.size() == output.size(), "Size mismatch");
  FieldElementT power = base;
  std::fill(output.begin(), output.end(), FieldElementT::One());

  // Find the number of bits to iterate on (maximal set bit index among exponents).
  const uint64_t exponents_or =
      std::accumulate(exponents.begin(), exponents.end(), 0, std::bit_or<>());
  const size_t n_bits = (exponents_or == 0 ? 0 : Log2Floor(exponents_or) + 1);

  // This implementation is a simple generalization of the modular exponentiation algorithm, using
  // the fact the computed powers of the base are independent of the exponent itself, thus applied
  // on a batch can save on computation. We iterate over the bits of all exponents, from the LSB to
  // the highest bit set. In every index we advance the 'power' by multiplying it by itself, and
  // update output[i] only is the current iteration bit is set in the i'th exponent.
  for (size_t bit_idx = 0; bit_idx < n_bits; bit_idx++) {
    const uint64_t test_mask = UINT64_C(1) << bit_idx;

    for (size_t i = 0; i < exponents.size(); ++i) {
      if ((exponents[i] & test_mask) != 0) {
        output[i] *= power;
      }
    }

    power *= power;
  }
}

/*
  Same as above, but allocates the memory for the output rather than getting it as a parameter.
*/
template <typename FieldElementT>
std::vector<FieldElementT> BatchPow(
    const FieldElementT& base, gsl::span<const uint64_t> exponents) {
  std::vector<FieldElementT> res = FieldElementT::UninitializedVector(exponents.size());
  BatchPow<FieldElementT>(base, exponents, res);
  return res;
}

/*
  Batch inverse (Montgomery).
  Motivation: Inverse in finite fields is usually much more expensive than multiplication (might be
  as bad as a factor of X10000 or even more). This technique allows inverting N field elements
  using only a single inverse operation, and additional 3(N-1) multiplications.

  The algorithm (given input a_1,a_2,...,a_n):

  1) Compute the sequence of partial products (b_1, b_2, ..., b_n) such that b_i = a_1 * ... * a_i.

  2) Compute the inverse of the entire product c=(b_n)^{-1}.

  3) Notice:
  3.a) (a_n)^{-1} = c*b_{n-1}
  3.b) (a_1 * ... * a_{n-1})^{-1} = (b_{n-1})^{-1} = c*a_n
  Use these two equations to iteratively compute the inverse of elements, starting of index n, and
  ending in index 1. Simply put, knowing the inverse of b_j allows computing both the inverse of a_j
  and the inverse of b_{j-1} using only two multiplication operations.
*/
template <typename FieldElementT>
void BatchInverse(
    gsl::span<const gsl::span<const FieldElementT>> input,
    gsl::span<const gsl::span<FieldElementT>> output) {
  if (input.empty()) {
    // Nothing to compute.
    LOG(INFO) << "Computing inverses of an empty batch.";
    return;
  }

  ASSERT_RELEASE(input.size() == output.size(), "Size mismatch.");
  for (size_t col = 0; col < input.size(); col++) {
    ASSERT_RELEASE(input[col].size() == output[col].size(), "Size mismatch.");
    if (!input[col].empty()) {
      ASSERT_RELEASE(input[col].data() != output[col].data(), "Inverse in place is not supported.");
    }
  }

  // Compute the sequence of partial products.
  FieldElementT elements_product = FieldElementT::One();
  const size_t n_cols = input.size();
  for (size_t col = 0; col < n_cols; col++) {
    for (size_t row = 0; row < input[col].size(); row++) {
      output[col][row] = elements_product;
      elements_product *= input[col][row];
    }
  }

  // Inverse the product of all elements.
  ASSERT_RELEASE(elements_product != FieldElementT::Zero(), "Batch to invert contains zero.");
  FieldElementT partial_prod_inv = elements_product.Inverse();

  // Compute the inverse for each element.
  for (size_t i = 1; i <= n_cols; ++i) {
    size_t col = n_cols - i;
    const size_t n_rows = input[col].size();
    for (size_t j = 1; j <= n_rows; j++) {
      size_t row = n_rows - j;
      output[col][row] = partial_prod_inv * output[col][row];
      partial_prod_inv *= input[col][row];
    }
  }
}

template <typename FieldElementT>
void BatchInverse(gsl::span<const FieldElementT> input, gsl::span<FieldElementT> output) {
  if (input.empty()) {
    // Nothing to compute.
    LOG(INFO) << "Computing inverses of an empty batch.";
    return;
  }
  ASSERT_RELEASE(input.data() != output.data(), "Inverse in place is not supported.");
  ASSERT_RELEASE(input.size() == output.size(), "Size mismatch.");

  std::array<const gsl::span<const FieldElementT>, 1> new_input = {input};
  std::array<const gsl::span<FieldElementT>, 1> new_output = {output};
  BatchInverse<FieldElementT>(gsl::make_span(new_input), gsl::make_span(new_output));
}

template <typename FieldElementT, size_t... I>
std::array<FieldElementT, sizeof...(I)> UninitializedFieldElementArrayImpl(
    std::index_sequence<I...> /*unused*/) {
  // We need to instantiate the array with sizeof...(I) copies of FieldElementT::Uninitialized().
  // To do it, we use the following trick:
  //
  // The expression (I, FieldElementT::Uninitialized()) simply evaluates to
  // FieldElementT::Uninitialized() (this is the comma operator in C++).
  // On the other hand, it depends on I, so adding the '...', evaluates it for every element of
  // I. Thus we obtain a list which is equivalent to:
  //   FieldElementT::Uninitialized(), FieldElementT::Uninitialized(), ...
  // where the number of instances is sizeof...(I).
  return {((void)I, FieldElementT::Uninitialized())...};
}

/*
  Returns an array of N uninitialized field elements.
*/
template <typename FieldElementT, size_t N>
std::array<FieldElementT, N> UninitializedFieldElementArray() {
  return UninitializedFieldElementArrayImpl<FieldElementT>(std::make_index_sequence<N>());
}

/*
  Returns the inner product of two given vectors.
*/
template <typename FieldElementT>
FieldElementT InnerProduct(
    gsl::span<const FieldElementT> vector_a, gsl::span<const FieldElementT> vector_b);

/*
  Performs a linear transformation (represented by a matrix) on a vector and stores the result at
  output.
*/
template <typename FieldElementT>
void LinearTransformation(
    gsl::span<const gsl::span<const FieldElementT>> matrix, gsl::span<const FieldElementT> vector,
    gsl::span<FieldElementT> output);

/*
  Calculates a linear combination of given vectors and given coefficients and stores the result at
  output.
*/
template <typename FieldElementT>
void LinearCombination(
    gsl::span<const FieldElementT> coefficients,
    gsl::span<const gsl::span<const FieldElementT>> vectors, gsl::span<FieldElementT> output);

}  // namespace starkware

#include "starkware/algebra/field_operations.inl"

#endif  // STARKWARE_ALGEBRA_FIELD_OPERATIONS_H_
