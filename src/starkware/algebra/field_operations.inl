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

namespace field_operations {
namespace details {

/*
  Returns the bits of the number (p-1)/2 where p is the size of the field.
*/
template <typename FieldElementT>
std::vector<bool> GetBitsOfHalfGroupSize() {
  using IntType = decltype(FieldElementT::FieldSize());
  IntType res, remainder;
  std::tie(res, remainder) =
      IntType::Div(IntType::Sub(FieldElementT::FieldSize(), IntType::One()).first, IntType(2));
  ASSERT_RELEASE(remainder == IntType::Zero(), "p - 1 must be divisible by 2.");
  return res.ToBoolVector();
}

}  // namespace details
}  // namespace field_operations

template <typename FieldElementT>
bool IsSquare(const FieldElementT& value) {
  if (value == FieldElementT::Zero()) {
    return true;
  }

  // value is a square if and only if value^((p-1) / 2) = 1.

  return Pow(value, field_operations::details::GetBitsOfHalfGroupSize<FieldElementT>()) ==
         FieldElementT::One();
}

template <typename FieldElementT>
FieldElementT Sqrt(const FieldElementT& value) {
  if (value == FieldElementT::Zero()) {
    return FieldElementT::Zero();
  }

  // We use the following algorithm to compute the square root of the element:
  // Let v be the input, let +r and -r be the roots of v and consider the ring
  //   R := F[x] / (x^2 - v).
  //
  // This ring is isomorphic to the ring F x F where the isomorphism is given by the map
  //   a*x + b --> (ar + b, -ar + b)  (recall that we don't know r, so we cannot compute this map).
  //
  // Pick a random element x + b in R, and compute (x + b)^((p-1)/2). Let's say that the result is
  // c*x + d.
  // Taking a random element in F to the power of (p-1)/2 gives +1 or -1 with probability
  // 0.5. Since R is isomorphic to F x F (where multiplication is pointwise), the result of the
  // computation will be one of the four pairs:
  //   (+1, +1), (-1, -1), (+1, -1), (-1, +1).
  //
  // If the result is (+1, +1) or (-1, -1) (which are the elements (0*x + 1) and (0*x - 1) in R) -
  // try again with another random element.
  //
  // If the result is (+1, -1) then cr + d = 1 and -cr + d = -1. Therefore r = c^{-1} and d=0. In
  // the second case -r = c^{-1}. In both cases c^{-1} will be the returned root.

  // Store an element in R as a pair: first * x + second.
  using RingElement = std::pair<FieldElementT, FieldElementT>;
  const RingElement one{FieldElementT::Zero(), FieldElementT::One()};
  const RingElement minus_one{FieldElementT::Zero(), -FieldElementT::One()};

  auto mult = [value](const RingElement& multiplier, RingElement* dst) {
    // Compute res * multiplier in the ring.
    auto res_first = multiplier.first * dst->second + multiplier.second * dst->first;
    auto res_second = multiplier.first * dst->first * value + multiplier.second * dst->second;
    *dst = {res_first, res_second};
  };

  Prng prng(std::array<std::byte, 0>{});

  // Compute q = (p - 1) / 2 and get its bits.
  const std::vector<bool> q_bits =
      field_operations::details::GetBitsOfHalfGroupSize<FieldElementT>();

  while (true) {
    // Pick a random element (x + b) in R.
    RingElement random_element{FieldElementT::One(), FieldElementT::RandomElement(&prng)};

    // Compute the exponentiation: random_element ^ ((p-1) / 2).
    RingElement res = GenericPow(random_element, q_bits, one, mult);

    // If res is either 1 or -1, try again.
    if (res == one || res == minus_one) {
      continue;
    }

    const FieldElementT root = res.first.Inverse();

    ASSERT_RELEASE(root * root == value, "value does not have a square root.");

    return root;
  }
}

template <typename FieldElementT>
FieldElementT InnerProduct(
    gsl::span<const FieldElementT> vector_a, gsl::span<const FieldElementT> vector_b) {
  ASSERT_RELEASE(
      vector_a.size() == vector_b.size(),
      "Length of vector_a must be equal to the length of vector_b");
  FieldElementT sum = FieldElementT::Zero();
  for (size_t i = 0; i < vector_a.size(); i++) {
    sum += vector_a[i] * vector_b[i];
  }
  return sum;
}

template <typename FieldElementT>
void LinearTransformation(
    gsl::span<const gsl::span<const FieldElementT>> matrix, gsl::span<const FieldElementT> vector,
    gsl::span<FieldElementT> output) {
  const size_t num_rows = matrix.size();
  ASSERT_RELEASE(output.size() == num_rows, "Output must be same dimension as input.");
  for (size_t i = 0; i < num_rows; ++i) {
    output[i] = InnerProduct(matrix[i], vector);
  }
}

template <typename FieldElementT>
void LinearCombination(
    gsl::span<const FieldElementT> coefficients,
    gsl::span<const gsl::span<const FieldElementT>> vectors, gsl::span<FieldElementT> output) {
  const size_t num_vectors = vectors.size();
  ASSERT_RELEASE(
      coefficients.size() == num_vectors, "Number of coefficients should match number of vectors.");
  ASSERT_RELEASE(num_vectors != 0, "Linear combination of empty list is not defined.");
  const size_t vec_length = vectors[0].size();
  for (const auto& vec : vectors) {
    ASSERT_RELEASE(vec.size() == vec_length, "Vectors must have same dimension.");
  }
  ASSERT_RELEASE(output.size() == vec_length, "Output must be same dimension as input.");
  std::fill(output.begin(), output.end(), FieldElementT::Zero());
  for (size_t i = 0; i < num_vectors; ++i) {
    for (size_t j = 0; j < vec_length; ++j) {
      output[j] += coefficients[i] * vectors[i][j];
    }
  }
}

}  // namespace starkware
