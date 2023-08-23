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

#ifndef STARKWARE_FRI_FRI_TEST_UTILS_H_
#define STARKWARE_FRI_FRI_TEST_UTILS_H_

#include <vector>

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polynomials.h"

namespace starkware {

namespace fri {
namespace details {

template <typename FieldElementT = TestFieldElement>
struct TestPolynomial {
  TestPolynomial(Prng* prng, int degree_bound)
      : coefs(GetRandomPolynomial<FieldElementT>(degree_bound - 1, prng)) {}

  /*
    Evaluates the polynomial at x.
  */
  FieldElementT EvalAt(FieldElementT x) { return HornerEval(x, coefs); }

  /*
    Evaluates the polynomial on the given domain.
  */
  std::vector<FieldElementT> GetData(
      const FftDomain<FftMultiplicativeGroup<FieldElementT>>& domain) {
    std::vector<FieldElementT> vec;
    for (const FieldElementT& x : domain) {
      vec.push_back(EvalAt(x));
    }
    return vec;
  }

  std::vector<FieldElementT> coefs;
};

}  // namespace details
}  // namespace fri

/*
  Given an evaluation of a polynomial on a coset, evaluates at a point.
  See remarks on ExtrapolatePointFromCoefficients().
*/
template <typename FieldElementT, typename BasesT>
FieldElementT ExtrapolatePoint(
    const BasesT& bases, const FieldElementVector& vec, const FieldElementT eval_point) {
  auto lde_manager = MakeLdeManager(bases);
  lde_manager->AddEvaluation(vec);
  FieldElementVector evaluation_results =
      FieldElementVector::MakeUninitialized<FieldElementT>(vec.Size());
  lde_manager->EvalOnCoset(
      FieldElement(eval_point), std::vector<FieldElementSpan>{evaluation_results.AsSpan()});
  return evaluation_results[0].As<FieldElementT>();
}

/*
  Given polynomial coefficients, evaluates at a point.
*/
template <typename FieldElementT, typename BasesT>
FieldElementT ExtrapolatePointFromCoefficients(
    const BasesT& bases, const std::vector<FieldElementT>& orig_coefs,
    const FieldElementT eval_point) {
  auto lde_manager = MakeLdeManager(bases);
  std::vector<FieldElementT> coefs(orig_coefs);
  ASSERT_RELEASE(coefs.size() <= bases[0].Size(), "Too many coefficients");
  coefs.resize(bases[0].Size(), FieldElementT::Zero());

  lde_manager->AddFromCoefficients(ConstFieldElementSpan(gsl::span<const FieldElementT>(coefs)));
  std::vector<FieldElementT> evaluation_results(bases[0].Size(), FieldElementT::Zero());
  lde_manager->EvalOnCoset(
      FieldElement(eval_point),
      std::vector<FieldElementSpan>{FieldElementSpan(gsl::make_span(evaluation_results))});
  return evaluation_results[0];
}

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_TEST_UTILS_H_
