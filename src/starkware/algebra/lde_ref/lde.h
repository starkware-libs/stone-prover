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

#ifndef STARKWARE_ALGEBRA_LDE_LDE_H_
#define STARKWARE_ALGEBRA_LDE_LDE_H_

#include <memory>
#include <utility>
#include <vector>

#include "starkware/algebra/domains/multiplicative_group.h"
#include "starkware/algebra/domains/ordered_group.h"
#include "starkware/algebra/fft/details.h"
#include "starkware/algebra/fft/multiplicative_group_ordering.h"
#include "starkware/algebra/polymorphic/field_element_span.h"
#include "starkware/fft_utils/fft_bases.h"

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

template <typename LdeT>
class LdeManagerTmpl;

class LdeManager {
 public:
  virtual ~LdeManager() = default;

  /*
    Adds an evaluation on coset that was used to build the LdeManager.
    Future EvalOnCoset invocations will add the lde of that evaluation to the results.
  */
  void AddEvaluation(FieldElementVector&& evaluation) {
    AddEvaluation(std::forward<FieldElementVector>(evaluation), nullptr);
  }
  void AddEvaluation(const ConstFieldElementSpan& evaluation) {
    AddEvaluation(evaluation, nullptr);
  }

  virtual void AddEvaluation(
      FieldElementVector&& evaluation, FftWithPrecomputeBase* fft_precomputed) = 0;
  virtual void AddEvaluation(
      const ConstFieldElementSpan& evaluation, FftWithPrecomputeBase* fft_precomputed) = 0;

  /*
    Evaluates the low degree extension of the evaluation that were previously added
    on a given coset.
    The results are ordered according to the order that the LDEs were added.
  */
  virtual void EvalOnCoset(
      const FieldElement& coset_offset, gsl::span<const FieldElementSpan> evaluation_results,
      FftWithPrecomputeBase* fft_precomputed) const = 0;

  virtual void EvalOnCoset(
      const FieldElement& coset_offset,
      gsl::span<const FieldElementSpan> evaluation_results) const = 0;

  /*
    Constructs an LDE from the coefficients of the polynomial (obtained by GetCoefficients()).
  */
  virtual void AddFromCoefficients(const ConstFieldElementSpan& coefficients) = 0;

  virtual void EvalAtPoints(
      size_t evaluation_idx, const ConstFieldElementSpan& points,
      const FieldElementSpan& outputs) const = 0;

  /*
    Returns the degree of the interpolation polynomial for Evaluation that was previously added
    through AddEvaluation().

    Returns -1 for the zero polynomial.
  */
  virtual int64_t GetEvaluationDegree(size_t evaluation_idx) const = 0;

  /*
    Returns the coefficients of the interpolation polynomial.

    Note that:
    1) The returned span is only valid while the lde_manger is alive.
    2) If the ordering of the LdeManager is kNaturalOrder, then the coefficients are in bit
       reversal order and vice versa.
  */
  virtual ConstFieldElementSpan GetCoefficients(size_t evaluation_idx) const = 0;

  /*
    Returns the domain of a single coset, which represents the order of the elements in a single
    coset evaluation.
    Gets the offset of the requested coset.
  */
  virtual std::unique_ptr<FftDomainBase> GetDomain(const FieldElement& offset) const = 0;

  virtual std::unique_ptr<FftWithPrecomputeBase> FftPrecompute(
      const FieldElement& coset_offset) const = 0;
  virtual std::unique_ptr<FftWithPrecomputeBase> IfftPrecompute() const = 0;

  template <typename LdeT>
  LdeManagerTmpl<LdeT>& As() {
    return dynamic_cast<LdeManagerTmpl<LdeT>&>(*this);
  }
};

std::unique_ptr<LdeManager> MakeLdeManager(const FftBases& bases);

std::unique_ptr<LdeManager> MakeLdeManager(
    const OrderedGroup& source_domain_group, const FieldElement& source_eval_coset_offset);

/*
  BitReversedOrderLdeManager is the same as LdeManager except input and output are in Bit Reversal
  Order.
*/
std::unique_ptr<LdeManager> MakeBitReversedOrderLdeManager(
    const OrderedGroup& source_domain_group, const FieldElement& source_eval_coset_offset);

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_LDE_LDE_H_
