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


#include "starkware/composition_polynomial/breaker.h"

namespace starkware {

namespace {

template <typename BasesT>
class PolynomialBreakTmpl {
  static_assert(sizeof(BasesT) == 0, "Not implemented for specified BasesT");
};

template <typename FieldElementT, MultiplicativeGroupOrdering Order>
class PolynomialBreakTmpl<MultiplicativeFftBases<FieldElementT, Order>> : public PolynomialBreak {
  using BasesT = MultiplicativeFftBases<FieldElementT, Order>;

 public:
  PolynomialBreakTmpl(const BasesT& bases, size_t log_breaks)
      : bases_(bases),
        log_breaks_(log_breaks),
        top_bases_(std::get<0>(BasesT::SplitDomain(bases[0], log_breaks))) {
    ASSERT_RELEASE(
        log_breaks <= bases.NumLayers(), "Number of breaks cannot be larger than coset size.");
  }

  // Polymorphic versions.
  std::vector<ConstFieldElementSpan> Break(
      const ConstFieldElementSpan& evaluation, const FieldElementSpan& output) const override {
    auto result =
        BreakTmpl(evaluation.template As<FieldElementT>(), output.template As<FieldElementT>());
    return std::vector<ConstFieldElementSpan>{result.begin(), result.end()};
  }

  FieldElement EvalFromSamples(
      const ConstFieldElementSpan& samples, const FieldElement& point) const override {
    return FieldElement(EvalFromSamplesTmpl(
        samples.template As<FieldElementT>(), point.template As<FieldElementT>()));
  }

 private:
  std::vector<gsl::span<const FieldElementT>> BreakTmpl(
      gsl::span<const FieldElementT> evaluation, gsl::span<FieldElementT> output) const {
    ASSERT_RELEASE(evaluation.size() == bases_[0].Size(), "Wrong size of evaluation");
    ASSERT_RELEASE(evaluation.size() == output.size(), "Wrong size of evaluation");
    const size_t n_breaks = Pow2(log_breaks_);
    const size_t chunk_size = evaluation.size() >> log_breaks_;

    FieldElementT correction_factor = FieldElementT::FromUint(n_breaks).Inverse();

    // Apply log_breaks_ layers of Ifft (instead of a full Ifft) to get the evaluations of the
    // h_i's.
    // NOLINTNEXTLINE: clang tidy constexpr bug.
    if constexpr (Order == MultiplicativeGroupOrdering::kBitReversedOrder) {
      std::vector<FieldElementT> temp = FieldElementT::UninitializedVector(evaluation.size());
      MultiplicativeIfft(bases_, evaluation, temp, log_breaks_);
      // Uninterleave.

      TaskManager::GetInstance().ParallelFor(
          chunk_size,
          [n_breaks, chunk_size, &temp, &output, &correction_factor](const TaskInfo& task_info) {
            for (size_t i = task_info.start_idx; i < task_info.end_idx; ++i) {
              for (size_t break_idx = 0; break_idx < n_breaks; ++break_idx) {
                output[break_idx * chunk_size + i] =
                    temp[i * n_breaks + break_idx] * correction_factor;
              }
            }
          },
          chunk_size);

    } else {  // NOLINT: clang tidy constexpr bug.
      MultiplicativeIfft(bases_, evaluation, output, log_breaks_);
      for (FieldElementT& y : output) {
        y *= correction_factor;
      }
    }

    std::vector<gsl::span<const FieldElementT>> results;
    results.reserve(n_breaks);
    for (size_t i = 0; i < n_breaks; ++i) {
      results.push_back(output.subspan(i * chunk_size, chunk_size));
    }

    return results;
  }

  FieldElementT EvalFromSamplesTmpl(
      gsl::span<const FieldElementT> samples, const FieldElementT& point) const {
    ASSERT_RELEASE(samples.size() == Pow2(log_breaks_), "Wrong size of samples");
    // NOLINTNEXTLINE: clang tidy constexpr bug.
    if constexpr (Order == MultiplicativeGroupOrdering::kBitReversedOrder) {
      return HornerEval(point, samples);
    } else {  // NOLINT: clang tidy constexpr bug.
      return HornerEvalBitReversed(point, samples);
    }
  }

  const BasesT bases_;
  const size_t log_breaks_;
  const BasesT top_bases_;
};

}  // namespace

std::unique_ptr<PolynomialBreak> MakePolynomialBreak(const FftBases& bases, size_t log_breaks) {
  return InvokeBasesTemplateVersion(
      [&](const auto& bases_inner) -> std::unique_ptr<PolynomialBreak> {
        using BasesT = std::remove_const_t<std::remove_reference_t<decltype(bases_inner)>>;
        return std::make_unique<PolynomialBreakTmpl<BasesT>>(bases_inner, log_breaks);
      },
      bases);
}

}  // namespace starkware
