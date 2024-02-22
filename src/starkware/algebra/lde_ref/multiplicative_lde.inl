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


#include "starkware/algebra/fft/fft_with_precompute.h"
#include "starkware/algebra/fft/multiplicative_group_ordering.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/lde/multiplicative_lde.h"
#include "starkware/algebra/polynomials.h"

namespace starkware {

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
auto MultiplicativeLde<Order, FieldElementT>::AddFromCoefficients(
    std::vector<FieldElementT>&& coefficients) -> MultiplicativeLde {
  return MultiplicativeLde(std::move(coefficients));
}

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
auto MultiplicativeLde<Order, FieldElementT>::AddFromEvaluation(
    const BasesT& bases, std::vector<FieldElementT>&& evaluation,
    FftWithPrecomputeBase* fft_precomputed) -> MultiplicativeLde {
  if (bases.NumLayers() > 0) {
    // We implement IFFT using the Dual Order FFT with w_inverse_ instead of w.

    using DualBasesT = decltype(GetDualBases(bases));
    MaybeOwnedPtr<FftWithPrecompute<DualBasesT>> maybe_precomputed =
        UseOwned(dynamic_cast<FftWithPrecompute<DualBasesT>*>(fft_precomputed));
    if (fft_precomputed == nullptr) {
      maybe_precomputed = UseMovedValue(FftWithPrecompute<DualBasesT>(GetDualBases(bases)));
    }
    (*maybe_precomputed).Fft(evaluation, evaluation);

    auto lde_size_inverse = FieldElementT::FromUint(evaluation.size()).Inverse();

    TaskManager& task_manager = TaskManager::GetInstance();
    task_manager.ParallelFor(
        evaluation.size(),
        [&evaluation, lde_size_inverse](const TaskInfo& task_info) {
          for (size_t i = task_info.start_idx; i < task_info.end_idx; i++) {
            evaluation[i] *= lde_size_inverse;
          }
        },
        evaluation.size());
  }
  return AddFromCoefficients(std::move(evaluation));
}

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
void MultiplicativeLde<Order, FieldElementT>::EvalAtCoset(
    const FftWithPrecompute<BasesT>& fft_precompute, gsl::span<FieldElementT> result) const {
  fft_precompute.Fft(polynomial_, result);
}

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
void MultiplicativeLde<Order, FieldElementT>::EvalAtPoints(
    gsl::span<FieldElementT> points, gsl::span<FieldElementT> outputs) const {
  switch (Order) {
    case MultiplicativeGroupOrdering::kBitReversedOrder:
      OptimizedBatchHornerEval<FieldElementT>(points, polynomial_, outputs);
      break;
    case MultiplicativeGroupOrdering::kNaturalOrder:
      // Natural Order Lde stores polynomial_ in BitReversed order.
      BatchHornerEvalBitReversed<FieldElementT>(points, polynomial_, outputs);
      break;
  }
}

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
int64_t MultiplicativeLde<Order, FieldElementT>::GetDegree() const {
  // Natural Order Lde stores polynomial_ in BitReversed order.
  bool bit_revese = Order == MultiplicativeGroupOrdering::kNaturalOrder;
  size_t log_n = SafeLog2(polynomial_.size());
  for (int64_t deg = polynomial_.size() - 1; deg >= 0; deg--) {
    if (polynomial_[bit_revese ? BitReverse(deg, log_n) : deg] != FieldElementT::Zero()) {
      return deg;
    }
  }
  return -1;
}

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
FftWithPrecompute<MultiplicativeFftBases<FieldElementT, Order>>
MultiplicativeLde<Order, FieldElementT>::FftPrecompute(
    const BasesT& bases, const FieldElementT& offset_compensation,
    const FieldElementT& new_offset) {
  return FftWithPrecompute<BasesT>(bases.GetShiftedBases(new_offset * offset_compensation));
}

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
std::unique_ptr<FftWithPrecomputeBase> MultiplicativeLde<Order, FieldElementT>::IfftPrecompute(
    const BasesT& bases) {
  using DualBasesT = decltype(GetDualBases(bases));
  FftWithPrecompute<DualBasesT>(GetDualBases(bases));
  return std::make_unique<FftWithPrecompute<DualBasesT>>(
      FftWithPrecompute<DualBasesT>(GetDualBases(bases)));
}

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
constexpr MultiplicativeGroupOrdering MultiplicativeLde<Order, FieldElementT>::DualOrder(
    MultiplicativeGroupOrdering order) {
  return order == MultiplicativeGroupOrdering::kNaturalOrder
             ? MultiplicativeGroupOrdering::kBitReversedOrder
             : MultiplicativeGroupOrdering::kNaturalOrder;
}

template <MultiplicativeGroupOrdering Order, typename FieldElementT>
auto MultiplicativeLde<Order, FieldElementT>::GetDualBases(const BasesT& domains) {
  switch (Order) {
    case MultiplicativeGroupOrdering::kBitReversedOrder:
      return MultiplicativeFftBases<FieldElementT, DualOrder(Order)>(
          domains[0].Basis().back().Inverse(), domains.NumLayers(), FieldElementT::One());
    case MultiplicativeGroupOrdering::kNaturalOrder:
      return MultiplicativeFftBases<FieldElementT, DualOrder(Order)>(
          domains[0].Basis().front().Inverse(), domains.NumLayers(), FieldElementT::One());
  }
}

}  // namespace starkware
