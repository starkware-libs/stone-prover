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

#ifndef STARKWARE_STARK_COMPOSITION_ORACLE_H_
#define STARKWARE_STARK_COMPOSITION_ORACLE_H_

#include <tuple>
#include <utility>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/algebra/domains/list_of_cosets.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/stark/committed_trace.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

/*
  Given committed traces, a mask and a composition polynomial, represents a virtual oracle of the
  composition polynomial over the mask over the concatenation of the traces.

  Trace i represents a list of width_i columns. The concatenation of the traces is a big trace of
  all the columns of all the traces in order. A mask is a list of pairs (offset, column_index).
  The evaluation of the composition polynomial at point x, is a function of
    (c_{column_index_i}[x * g^offset_i]) for i = 0..mask_size - 1
  This will be called the mask (over the columns c_*) at point x.
  To evaluate at point x, the virtual oracle needs to decommit the mask at point x, from all the
  traces, since the columns are spread over the provided n_traces traces.
*/
class CompositionOracleProver {
 public:
  CompositionOracleProver(
      MaybeOwnedPtr<const ListOfCosets> evaluation_domain,
      std::vector<MaybeOwnedPtr<CommittedTraceProverBase>> traces,
      gsl::span<const std::pair<int64_t, uint64_t>> mask, MaybeOwnedPtr<const Air> air,
      MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial, ProverChannel* channel);

  /*
    Evaluates the composition polynomial over d cosets, where d is the degree bound of the
    composition polynomial over the trace length.

    The evaluation is done in task_size tasks. This is forwarded to the composition polynomial
    EvalOnCosetBitReversedOutput, see more info there.
  */
  FieldElementVector EvalComposition(uint64_t task_size) const;

  /*
    Given queries for the virtual oracle, decommits the correct values from the traces to prove the
    virtual oracle values at these queries.
  */
  void DecommitQueries(const std::vector<std::pair<uint64_t, uint64_t>>& queries) const;

  /*
    Computes the mask of the trace columns at a point.
    WARNING: This function introduces overheads (polymorphism), and should not be used at
    performance critical areas. Its purpose is for out of domain sampling.
  */
  void EvalMaskAtPoint(const FieldElement& point, const FieldElementSpan& output) const;

  /*
    Composition polynomial degree divided by trace length.
  */
  uint64_t ConstraintsDegreeBound() const;

  const ListOfCosets& EvaluationDomain() const { return *evaluation_domain_; }

  gsl::span<const std::pair<int64_t, uint64_t>> GetMask() const { return mask_; }

  /*
    Total number of columns in all the traces combined.
  */
  size_t Width() const;

  std::vector<const CommittedTraceProverBase*> Traces() const;

  std::vector<MaybeOwnedPtr<CommittedTraceProverBase>> MoveTraces() && {
    return std::move(traces_);
  }

 private:
  std::vector<MaybeOwnedPtr<CommittedTraceProverBase>> traces_;
  std::vector<std::pair<int64_t, uint64_t>> mask_;
  std::vector<std::vector<std::pair<int64_t, uint64_t>>> split_masks_;
  MaybeOwnedPtr<const ListOfCosets> evaluation_domain_;
  MaybeOwnedPtr<const Air> air_;
  MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial_;
  ProverChannel* const channel_;
};

class CompositionOracleVerifier {
 public:
  CompositionOracleVerifier(
      MaybeOwnedPtr<const ListOfCosets> evaluation_domain,
      std::vector<MaybeOwnedPtr<const CommittedTraceVerifierBase>> traces,
      gsl::span<const std::pair<int64_t, uint64_t>> mask, MaybeOwnedPtr<const Air> air,
      MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial, VerifierChannel* channel);

  FieldElementVector VerifyDecommitment(std::vector<std::pair<uint64_t, uint64_t>> queries) const;

  /*
    Composition polynomial degree divided by trace length.
  */
  uint64_t ConstraintsDegreeBound() const;

  const ListOfCosets& EvaluationDomain() const { return *evaluation_domain_; }

  gsl::span<const std::pair<int64_t, uint64_t>> GetMask() const { return mask_; }

  const CompositionPolynomial& GetCompositionPolynomial() const { return *composition_polynomial_; }

  /*
    Total number of columns in all the traces combined.
  */
  size_t Width() const;

  std::vector<MaybeOwnedPtr<const CommittedTraceVerifierBase>> MoveTraces() && {
    return std::move(traces_);
  }

 private:
  std::vector<MaybeOwnedPtr<const CommittedTraceVerifierBase>> traces_;
  std::vector<std::pair<int64_t, uint64_t>> mask_;
  std::vector<std::vector<std::pair<int64_t, uint64_t>>> split_masks_;
  MaybeOwnedPtr<const ListOfCosets> evaluation_domain_;
  MaybeOwnedPtr<const Air> air_;
  MaybeOwnedPtr<const CompositionPolynomial> composition_polynomial_;
  VerifierChannel* const channel_;
};

}  // namespace starkware

#endif  // STARKWARE_STARK_COMPOSITION_ORACLE_H_
