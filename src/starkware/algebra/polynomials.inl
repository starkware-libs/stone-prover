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

#include "starkware/algebra/field_operations.h"
#include "starkware/utils/bit_reversal.h"
#include "starkware/utils/task_manager.h"

#include <mutex>
#include <unordered_map>

namespace starkware {

template <typename FieldElementT>
std::vector<FieldElementT> LagrangeInterpolation(
    gsl::span<const FieldElementT> domain, gsl::span<const FieldElementT> values) {
  // Given a domain x_0, x_1, ... ,x_n , the Lagrange basis is the set of polynomials
  // L_0(x), L_1(x), ... , L_n(x), where L_i(x) = \prod_{i != j} (x - x_j) / (x_i - x_j).
  // Given sequence of values y_0, y_1, ... , y_n, the Lagrange interpolation is
  // the polynomial p(x) = \sum_{i=0}^n y_i * L_i(x). p(x) is the only polynomial of degree at most
  // 'n' such that p(x_i) - y_i for all 0<= i <= n.

  ASSERT_RELEASE(domain.size() == values.size(), "Size mismatch.");
  const size_t size = domain.size();

  // Initializing the result polynomial, which will eventually hold the result, by iteratively
  // adding to it the Lagrange polynomials (L_i), multiplied by the values (y_i).
  std::vector<FieldElementT> result(size, FieldElementT::Zero());

  // Define the constant '1' polynomial, with first coefficient being 1, and the rest are zeros.
  std::vector<FieldElementT> poly_const_one({FieldElementT::One()});
  poly_const_one.resize(size, FieldElementT::Zero());

  // Define the vector representing the numerator polynomial. Done outside the loop to
  // prevent repeatedly allocating memory inside the loop (allocation is done only in first
  // iteration).
  std::vector<FieldElementT> numerator;

  // Each loop iteration i computes the coefficients of L_i(x) and adds
  // y_i * L_i(x) to the result polynomial.
  // We divide the construction of L_i(x) to the iterative construction of the numerator
  // N(x) = \prod_{i != j} (x - x_j) and the denominator D = \prod_{i != j} (x_i - x_j), notice D is
  // a field element. We eventually add  y_i/D * N(x) to the result.
  for (size_t i = 0; i < size; ++i) {
    // Initialize numerator to be the constant '1' polynomial.
    numerator.assign(poly_const_one.begin(), poly_const_one.end());

    // Initialize the denominator to be the constant field element 1.
    FieldElementT denominator = FieldElementT::One();

    // Loop on all j != i.
    for (size_t j = 0; j < size; j++) {
      if (j == i) {
        continue;
      }

      // Add current factor to denominator.
      denominator *= (domain[i] - domain[j]);

      // Add current factor to numerator, using the equation:
      // (\sum a_k * x^k)(x-d) = \sum (a_{k-1} - d*a_k) * x^k
      // (assuming a_{-1} = 0) for any sequence {a_k} and constant d.
      // Notice that after 'j' multiplications, the degree of the product is at most 'j'
      // (represented by j+1 coefficients).
      const size_t prod_degree = (j < i ? j + 1 : j);
      for (size_t k = prod_degree; k > 0; k--) {
        numerator[k] = numerator[k - 1] - domain[j] * numerator[k];
      }
      numerator[0] = -(domain[j] * numerator[0]);
    }

    // Compute y_i / D.
    const FieldElementT factor = values[i] * denominator.Inverse();

    // Add y_i/D * N(x) to result.
    for (size_t c = 0; c < size; c++) {
      result[c] = result[c] + factor * numerator[c];
    }
  }

  return result;
}

template <typename FieldElementT>
std::vector<FieldElementT> GetRandomPolynomial(size_t deg, Prng* prng) {
  std::vector<FieldElementT> coefs;
  for (size_t i = 0; i < deg; ++i) {
    coefs.push_back(FieldElementT::RandomElement(prng));
  }
  coefs.push_back(RandomNonZeroElement<FieldElementT>(prng));
  return coefs;
}

template <typename FieldElementT>
FieldElementT HornerEval(const FieldElementT& x, const std::vector<FieldElementT>& coefs) {
  return HornerEval(x, gsl::make_span(coefs));
}

template <typename FieldElementT>
FieldElementT HornerEval(const FieldElementT& x, gsl::span<const FieldElementT> coefs) {
  FieldElementT res = FieldElementT::Uninitialized();
  BatchHornerEval<FieldElementT>({x}, coefs, gsl::make_span(&res, 1));
  return res;
}

template <typename FieldElementT>
void BatchHornerEval(
    gsl::span<const FieldElementT> points, gsl::span<const FieldElementT> coefs,
    gsl::span<FieldElementT> outputs, size_t stride) {
  ASSERT_RELEASE(
      outputs.size() == points.size() * stride,
      "The number of outputs must be (number of points) * stride.");
  for (FieldElementT& res : outputs) {
    res = FieldElementT::Zero();
  }
  for (auto it = coefs.rbegin(); it != coefs.rend();) {
    for (size_t s = 0; s < stride; ++s, ++it) {
      for (size_t point_idx = 0; point_idx < points.size(); point_idx++) {
        auto& output = outputs[point_idx * stride + stride - 1 - s];
        output = output * points[point_idx] + *it;
      }
    }
  }
}

template <typename FieldElementT>
void ParallelBatchHornerEval(
    gsl::span<const FieldElementT> points, gsl::span<const FieldElementT> coefs,
    gsl::span<FieldElementT> outputs, size_t stride, size_t min_chunk_size) {
  std::mutex output_lock;

  ASSERT_RELEASE(
      outputs.size() == points.size() * stride,
      "The number of outputs must be the same as the number of points.");

  for (FieldElementT& res : outputs) {
    res = FieldElementT::Zero();
  }

  const size_t n_coefs_in_polynomial = SafeDiv(coefs.size(), stride);

  TaskManager::GetInstance().ParallelFor(
      n_coefs_in_polynomial,
      [&output_lock, outputs, coefs, points, stride](const TaskInfo& task_info) {
        const size_t start_idx = task_info.start_idx;
        const size_t end_idx = task_info.end_idx;
        std::vector<FieldElementT> partial_outputs(outputs.size(), FieldElementT::Zero());

        BatchHornerEval<FieldElementT>(
            points, coefs.subspan(start_idx * stride, (end_idx - start_idx) * stride),
            partial_outputs, stride);

        for (size_t point_idx = 0; point_idx < points.size(); point_idx++) {
          const FieldElementT point_pow = Pow(points[point_idx], start_idx);
          for (size_t j = 0; j < stride; j++) {
            partial_outputs[stride * point_idx + j] *= point_pow;
          }
        }

        std::unique_lock lock(output_lock);
        for (size_t output_idx = 0; output_idx < outputs.size(); output_idx++) {
          outputs[output_idx] += partial_outputs[output_idx];
        }
      },
      n_coefs_in_polynomial, min_chunk_size);
}

template <typename FieldElementT>
void OptimizedBatchHornerEval(
    gsl::span<const FieldElementT> points, gsl::span<const FieldElementT> coefs,
    gsl::span<FieldElementT> outputs, size_t stride) {
  // Don't try to optimize in the following cases:
  // 1. Small polynomial (less than 1024 coefficients) - to ensure that the preparation for the
  // optimization itself will not take more time than the naive evaluation.
  // 2. Stride becomes too large.
  // 3. Number of effective coefficients is odd.
  const size_t n_effective_coefs = SafeDiv(coefs.size(), stride);
  if (coefs.size() < 1024 || stride > Pow2(15) || n_effective_coefs % 2 == 1) {
    ParallelBatchHornerEval(points, coefs, outputs, stride);
    return;
  }

  // Find which of the points have identical squares (x and -x).
  // Map from x^2 to indices to the points vector.
  std::unordered_map<std::string, std::vector<size_t>> point_squares;

  // Use ToString() as a way to get hashable object from FieldElementT.
  for (size_t i = 0; i < points.size(); ++i) {
    const FieldElementT& point = points[i];
    point_squares[(point * point).ToString()].push_back(i);
  }
  const size_t n_squares = point_squares.size();

  // If we got more than 2/3 of the original points, don't optimize.
  // Usually when we can optimize we get an exact ratio of 1/2 since points are closed under
  // negation.
  if (n_squares > points.size() * 2 / 3) {
    ParallelBatchHornerEval(points, coefs, outputs, stride);
    return;
  }

  VLOG(2) << "Applying coset optimization. Number of points: " << points.size()
          << ", after squaring: " << n_squares;

  // Create a list of squares in the same order of iterating point_squares.
  std::vector<FieldElementT> squares;
  squares.reserve(n_squares);
  for (const auto& pair : point_squares) {
    const std::vector<size_t>& indices_vec = pair.second;
    squares.push_back(points[indices_vec[0]] * points[indices_vec[0]]);
  }

  // Treat coefs as twice the number of polynomials, so that each polynomial is split as follows:
  // f(x) = g(x^2) + x h(x^2). Substitute squares in these polynomials.
  auto partial_outputs = FieldElementT::UninitializedVector(n_squares * stride * 2);
  OptimizedBatchHornerEval<FieldElementT>(squares, coefs, partial_outputs, stride * 2);

  // Reconstruct the values of the original polynomials.
  size_t i = 0;
  for (const auto& pair : point_squares) {
    const std::vector<size_t>& indices_vec = pair.second;
    for (size_t idx : indices_vec) {
      for (size_t j = 0; j < stride; j++) {
        const auto& x = points[idx];
        const auto& g_of_x_squared = partial_outputs[stride * 2 * i + j];
        const auto& h_of_x_squared = partial_outputs[stride * (2 * i + 1) + j];
        outputs[stride * idx + j] = g_of_x_squared + x * h_of_x_squared;
      }
    }
    i++;
  }
}

template <typename FieldElementT>
FieldElementT HornerEvalBitReversed(
    const FieldElementT& x, const std::vector<FieldElementT>& coefs) {
  return HornerEvalBitReversed(x, gsl::make_span(coefs));
}

template <typename FieldElementT>
FieldElementT HornerEvalBitReversed(const FieldElementT& x, gsl::span<const FieldElementT> coefs) {
  FieldElementT res = FieldElementT::Uninitialized();
  BatchHornerEvalBitReversed<FieldElementT>({x}, coefs, gsl::make_span(&res, 1));
  return res;
}

template <typename FieldElementT>
void BatchHornerEvalBitReversed(
    gsl::span<const FieldElementT> points, gsl::span<const FieldElementT> coefs,
    gsl::span<FieldElementT> outputs) {
  ASSERT_RELEASE(
      points.size() == outputs.size(),
      "The number of outputs must be the same as the number of points.");
  const size_t n_coefs = coefs.size();
  const size_t log_n_coefs = SafeLog2(n_coefs);

  for (FieldElementT& res : outputs) {
    res = FieldElementT::Zero();
  }
  for (size_t coef_idx = 0; coef_idx < n_coefs; coef_idx++) {
    for (size_t point_idx = 0; point_idx < points.size(); point_idx++) {
      outputs[point_idx] = outputs[point_idx] * points[point_idx] +
                           coefs[BitReverse(n_coefs - 1 - coef_idx, log_n_coefs)];
    }
  }
}

}  // namespace starkware
