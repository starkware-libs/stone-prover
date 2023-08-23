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

#ifndef STARKWARE_COMPOSITION_POLYNOMIAL_PERIODIC_COLUMN_H_
#define STARKWARE_COMPOSITION_POLYNOMIAL_PERIODIC_COLUMN_H_

#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/lde/lde.h"
#include "starkware/algebra/lde/lde_manager_impl.h"
#include "starkware/algebra/polynomials.h"

namespace starkware {

/*
  Represents a polynomial whose evaluation on a given coset is periodic with a given period.
  This can be used to simulate public columns (known both to the prover and the verifier) where the
  data of the column is periodic with a relatively small period. For example, round constants that
  appear in a hash function and repeat every invocation.

  Example usage:
    PeriodicColumn p = ..;
    auto coset_eval = p.GetCoset(..);
    ParallelFor(..., {
      auto it = coset_eval.begin();
      // Do stuff with iterator, safely
    }).
*/
template <typename FieldElementT>
class PeriodicColumn {
 public:
  /*
    Constructs a PeriodicColumn whose evaluations on the coset offset*<group_generator> is composed
    of repetitions of the given values.
    Namely, f(offset * group_generator^i) = values[i % values.size()].
  */
  PeriodicColumn(
      gsl::span<const FieldElementT> values, const FieldElementT& group_generator,
      const FieldElementT& offset, uint64_t coset_size, uint64_t column_step);

  FieldElementT EvalAtPoint(const FieldElementT& x) const;

  /*
    Returns the actual degree of the interpolant.
  */
  int64_t GetActualDegree() const { return lde_manager_.GetEvaluationDegree(0); }

  // Forward declaration.
  class CosetEvaluation;

  /*
    Returns a generator that computes the polynomial on a coset.
  */
  CosetEvaluation GetCoset(const FieldElementT& start_point, size_t coset_size) const;

 private:
  const FieldElementT group_generator_;

  /*
    Defines the set of rows on which the values of the periodic column will be written. The set will
    be
      { i * column_step : i = 0, 1, 2, ... }.
  */
  const uint64_t column_step_;

  /*
    The period of the column with respect to the trace (and not with respect to the virtual column).
    Note that
      period_in_trace_ = column_step_ * values.size().
  */
  const uint64_t period_in_trace_;

  /*
    The size of the coset divided by the length of the period.
  */
  const size_t n_copies_;

  /*
    The lde_manager of the column. This should be treated as a polynomial in x^{n_copies}.
  */
  LdeManagerTmpl<MultiplicativeLde<MultiplicativeGroupOrdering::kNaturalOrder, FieldElementT>>
      lde_manager_;
};

/*
  Represents an efficient evaluation of the periodic column on a coset. Can spawn thin iterators to
  thie evaluation, which are thread-safe.
*/
template <typename FieldElementT>
class PeriodicColumn<FieldElementT>::CosetEvaluation {
 public:
  explicit CosetEvaluation(std::vector<FieldElementT> values)
      : values_(std::move(values)), index_mask_(values_.size() - 1) {
    ASSERT_RELEASE(
        IsPowerOfTwo(values_.size()), "Currently values must be of size which is a power of two.");
  }

  class Iterator {
   public:
    Iterator(const CosetEvaluation* parent, uint64_t index, const uint64_t index_mask)
        : parent_(parent), index_(index), index_mask_(index_mask) {}

    Iterator& operator++() {
      index_ = (index_ + 1) & index_mask_;
      return *this;
    }

    Iterator operator+(uint64_t offset) const {
      return Iterator(parent_, (index_ + offset) & index_mask_, index_mask_);
    }

    FieldElementT operator*() const { return parent_->values_[index_]; }

   private:
    const CosetEvaluation* parent_;
    uint64_t index_;
    const uint64_t index_mask_;
  };

  Iterator begin() const { return Iterator(this, 0, index_mask_); }  // NOLINT

 private:
  const std::vector<FieldElementT> values_;
  const uint64_t index_mask_;
};

}  // namespace starkware

#include "starkware/composition_polynomial/periodic_column.inl"

#endif  // STARKWARE_COMPOSITION_POLYNOMIAL_PERIODIC_COLUMN_H_
