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

#ifndef STARKWARE_STARK_COMMITTED_TRACE_H_
#define STARKWARE_STARK_COMMITTED_TRACE_H_

#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/trace.h"
#include "starkware/algebra/domains/list_of_cosets.h"
#include "starkware/algebra/lde/cached_lde_manager.h"
#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {

/*
  Given a Trace (a vector of column evaluations over the trace domain), responsible for the LDE over
  the evaluation domain, and for the commitment of that LDE. Commitment is done over the evaluation
  domain, where each commitment row is of size n_columns.
*/
class CommittedTraceProverBase {
 public:
  virtual ~CommittedTraceProverBase() = default;

  virtual size_t NumColumns() const = 0;
  virtual CachedLdeManager* GetLde() = 0;

  /*
    Commits on the LDE. If bit_reverse is true, the trace is bit reversed with respect to the
    provided trace_domain, which means we need to bit reverse the column before the LDE.

    Assumption: trace_domain is a shift of evaluation_domain.Bases().
  */
  virtual void Commit(Trace&& trace, const FftBases& trace_domain, bool bit_reverse) = 0;

  /*
    Given queries for the commitment, computes the relevant commitment leaves from the LDE, and
    decommits them. queries is a list of tuples (coset_index, offset, column_index) for the elements
    that need to be decommitted.
  */
  virtual void DecommitQueries(
      gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const = 0;

  /*
    Computes the mask of the trace columns at a point.
    WARNING: This function introduces overheads (polymorphism), and should not be used at
    performance critical areas. Its purpose is for out of domain sampling.
  */
  virtual void EvalMaskAtPoint(
      gsl::span<const std::pair<int64_t, uint64_t>> mask, const FieldElement& point,
      const FieldElementSpan& output) const = 0;

  /*
    Call to finalize LDE evaluations. For example, you will not be able to call
    GetLde()->EvalAtPointsNotCached().
  */
  virtual void FinalizeEval() = 0;
};

/*
  Given a Trace (a vector of column evaluations over the trace domain), responsible for the LDE
  over the evaluation domain, and for the commitment of that LDE. Commitment is done over the
  evaluation domain, where each commitment row is of size n_columns.
*/
class CommittedTraceProver : public CommittedTraceProverBase {
 public:
  // Constructor.
  CommittedTraceProver(
      const CachedLdeManager::Config& cached_lde_config,
      MaybeOwnedPtr<const ListOfCosets> evaluation_domain, size_t n_columns,
      const TableProverFactory& table_prover_factory);

  size_t NumColumns() const override { return n_columns_; }
  CachedLdeManager* GetLde() override { return lde_.get(); }

  void Commit(Trace&& trace, const FftBases& trace_domain, bool bit_reverse) override;

  void DecommitQueries(
      gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const override;

  void EvalMaskAtPoint(
      gsl::span<const std::pair<int64_t, uint64_t>> mask, const FieldElement& point,
      const FieldElementSpan& output) const override;

  void FinalizeEval() override { lde_->FinalizeEvaluations(); }

 private:
  /*
    Given commitment row indices, computes the rows using lde_. output must be of size
    NumColumns(), and each FieldElementVector inside will be filled with the column evaluations at
    given rows.
  */
  void AnswerQueries(
      const std::vector<uint64_t>& rows_to_fetch, std::vector<FieldElementVector>* output) const;

  CachedLdeManager::Config cached_lde_config_;
  std::unique_ptr<CachedLdeManager> lde_;
  MaybeOwnedPtr<const ListOfCosets> evaluation_domain_;
  size_t n_columns_;
  std::unique_ptr<TableProver> table_prover_;
};

class CommittedTraceVerifierBase {
 public:
  virtual ~CommittedTraceVerifierBase() = default;

  virtual size_t NumColumns() const = 0;

  /*
    Verifier side of Commit().
  */
  virtual void ReadCommitment() = 0;

  /*
    Verifier side of DecommitQueries().
  */
  virtual FieldElementVector VerifyDecommitment(
      gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const = 0;
};

class CommittedTraceVerifier : public CommittedTraceVerifierBase {
 public:
  /*
    The parameter should_verify_base_field is relevant when using an extension field. It indicates
    that the verifier should verify that the field element queries of the trace are in the base
    field.
  */
  CommittedTraceVerifier(
      MaybeOwnedPtr<const ListOfCosets> evaluation_domain, size_t n_columns,
      const TableVerifierFactory& table_verifier_factory, bool should_verify_base_field = false);

  size_t NumColumns() const override { return n_columns_; }

  void ReadCommitment() override;

  FieldElementVector VerifyDecommitment(
      gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) const override;

 private:
  MaybeOwnedPtr<const ListOfCosets> evaluation_domain_;
  const size_t n_columns_;
  std::unique_ptr<TableVerifier> table_verifier_;

  /*
    True iff the verifier should verify that the corresponding trace elements are base field
    elements (relevant only in case that the proof is done over an extension field).
  */
  const bool should_verify_base_field_;
};

}  // namespace starkware

#endif  // STARKWARE_STARK_COMMITTED_TRACE_H_
