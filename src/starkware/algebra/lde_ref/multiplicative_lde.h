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

#ifndef STARKWARE_ALGEBRA_LDE_MULTIPLICATIVE_LDE_H_
#define STARKWARE_ALGEBRA_LDE_MULTIPLICATIVE_LDE_H_

#include <memory>
#include <utility>
#include <vector>

#include "starkware/algebra/fft/fft_with_precompute.h"
#include "starkware/algebra/fft/multiplicative_group_ordering.h"

namespace starkware {

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
class MultiplicativeLde {
 public:
  using BasesT = MultiplicativeFftBases<FieldElementT, Order>;
  using T = FieldElementT;
  static constexpr MultiplicativeGroupOrdering kOrder = Order;
  using PrecomputeType = FftWithPrecompute<BasesT>;

  /*
    Constructs an LDE from the coefficients of the polynomial (obtained by GetCoefficients()).
  */
  static MultiplicativeLde AddFromCoefficients(std::vector<FieldElementT>&& coefficients);

  /*
    Constructs an LDE from the evaluation of the polynomial on the domain bases[0].
  */
  static MultiplicativeLde AddFromEvaluation(
      const BasesT& bases, std::vector<FieldElementT>&& evaluation,
      FftWithPrecomputeBase* fft_precomputed);

  void EvalAtCoset(
      const FftWithPrecompute<BasesT>& fft_precompute, gsl::span<FieldElementT> result) const;

  void EvalAtPoints(gsl::span<FieldElementT> points, gsl::span<FieldElementT> outputs) const;

  int64_t GetDegree() const;

  const std::vector<FieldElementT>& GetCoefficients() const { return polynomial_; }

  static FftWithPrecompute<BasesT> FftPrecompute(
      const BasesT& bases, const FieldElementT& offset_compensation,
      const FieldElementT& new_offset);
  static std::unique_ptr<FftWithPrecomputeBase> IfftPrecompute(const BasesT& bases);

 private:
  static constexpr MultiplicativeGroupOrdering DualOrder(MultiplicativeGroupOrdering order);

  static auto GetDualBases(const BasesT& domains);

  explicit MultiplicativeLde(std::vector<FieldElementT>&& polynomial)
      : polynomial_(std::move(polynomial)) {}

  // polynomial_ holds the coefficients of P(c*x) where c = offset_compensation_.Inverse().
  // The order of coefficients is the 'opposite' (== dual) of the order defined by the template
  // parameter 'Order'.
  // If 'Order' is kBitReversedOrder then the coefficients are in Natural order.
  // Otherwise, the coefficients are in bit reversed order.
  std::vector<FieldElementT> polynomial_;
};

}  // namespace starkware

#include "starkware/algebra/lde/multiplicative_lde.inl"

#endif  // STARKWARE_ALGEBRA_LDE_MULTIPLICATIVE_LDE_H_
