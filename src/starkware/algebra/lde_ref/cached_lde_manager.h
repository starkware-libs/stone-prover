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

#ifndef STARKWARE_ALGEBRA_LDE_CACHED_LDE_MANAGER_H_
#define STARKWARE_ALGEBRA_LDE_CACHED_LDE_MANAGER_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "starkware/algebra/lde/lde.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

class CachedLdeManager {
 private:
  /*
    LdeCacheEntry is a vector of n_columns_ FieldElementVectors, and each one has the size of the
    coset. First index is the column, second index is the entry in the coset.
  */
  using LdeCacheEntry = std::vector<FieldElementVector>;

 public:
  struct Config {
    /*
      Controls a Memory/Performance tradeoff. Setting this value to false, reduces the memory
      consumption by recomputing the LDE when needed instead of storing it.
    */
    bool store_full_lde;

    /*
      Setting this value to true will recompute the LDE (using FFT on the entire coset) when
      evaluating at a point. Otherwise, only the evaluations required will be computed (using horner
      evaluation). This value has no effect when store_full_lde is true.
    */
    bool use_fft_for_eval;
  };

  CachedLdeManager(
      const Config& config, MaybeOwnedPtr<LdeManager> lde_manager,
      MaybeOwnedPtr<FieldElementVector> coset_offsets)
      : lde_manager_(std::move(lde_manager)),
        coset_offsets_(std::move(coset_offsets)),
        config_(config),
        cache_(coset_offsets_->Size()),
        ifft_precompute_(lde_manager_->IfftPrecompute()),
        previous_coset_offset_(coset_offsets_->At(0)) {
    ASSERT_RELEASE(coset_offsets_->Size() > 0, "At least one coset offset required");
    domain_size_ = lde_manager_->GetDomain(coset_offsets_->At(0))->Size();
  }

  void AddEvaluation(FieldElementVector&& evaluation) {
    ASSERT_RELEASE(!done_adding_, "Cannot call AddEvaluation after EvalOnCoset.");
    lde_manager_->AddEvaluation(std::move(evaluation), ifft_precompute_.get());
    n_columns_++;
  }

  void AddEvaluation(const ConstFieldElementSpan& evaluation) {
    ASSERT_RELEASE(!done_adding_, "Cannot call AddEvaluation after EvalOnCoset.");
    lde_manager_->AddEvaluation(evaluation, ifft_precompute_.get());
    n_columns_++;
  }

  /*
    Allocates a storage entry, to avoid allocations in succeeding uses of EvalOnCost. Will return
    nullptr, if store_full_lde is true.

    Example usage:
      auto storage = cached_lde_manager_->AllocateStorage();
      for (...) {
        auto result = cached_lde_manager_->EvalOnCoset(coset_index, storage.get());
      }
    .
  */
  std::unique_ptr<LdeCacheEntry> AllocateStorage() const;

  /*
    Evaluates an entire coset.
    If this coset is cached, a pointer to the cache is returned. Otherwise, a pointer to the storage
    is returned.
  */
  const LdeCacheEntry* EvalOnCoset(uint64_t coset_index, LdeCacheEntry* storage);

  /*
    Evaluates all columns at point. Cached version, takes pairs of (coset_index, point_index).
  */
  void EvalAtPoints(
      gsl::span<const std::pair<uint64_t, uint64_t>> coset_and_point_indices,
      gsl::span<const FieldElementSpan> outputs);

  /*
    Evaluates all columns at point. Not cached version, takes FieldElements.
  */
  void EvalAtPointsNotCached(
      size_t column_index, const ConstFieldElementSpan& points, const FieldElementSpan& output);

  /*
    Indicates no new computations will occur. If store_full_lde_ is true, that means we can release
    lde_manager_ if owned.
    New computations refers to EvalOnCoset() or EvalAtPoints() on a new coset, or
    EvalAtPointsNotCached().
  */
  void FinalizeEvaluations();

  /*
    Indicates AddEvaluation() will not be called anymore.
  */
  void FinalizeAdding() {
    ASSERT_RELEASE(!done_adding_, "FinalizeAdding called twice.");
    ifft_precompute_.reset(nullptr);
    fft_precompute_ = lde_manager_->FftPrecompute(coset_offsets_->At(0));

    done_adding_ = true;
  }
  /*
    Returns the number of columns in CachedLdeManager. Also, stops adding new evaluations, to make
    sure the number of columns won't change. This is why this is not const.
  */
  size_t NumColumns() const {
    ASSERT_RELEASE(done_adding_, "NumColumns() must be called after calling FinalizeAdding().");
    return n_columns_;
  }

  bool IsCached() const { return config_.store_full_lde; }

 private:
  /*
    Allocates a new entry, ready to be filled.
  */
  LdeCacheEntry InitializeEntry() const;

  MaybeOwnedPtr<LdeManager> lde_manager_;
  MaybeOwnedPtr<FieldElementVector> coset_offsets_;
  uint64_t domain_size_;
  bool done_adding_ = false;
  size_t n_columns_ = 0;
  Config config_;

  /*
    Cache entries. Optional, since we can have entries which were not computed yet.
  */
  std::vector<std::optional<LdeCacheEntry>> cache_;

  /*
    Saves precompute (which contains twiddle factors) and previous coset offset, in order to quickly
    update the previous twiddle factors to the new twiddle factors. This is done by multiplying the
    previous twiddle factors by (offset/previous)^k where k varies according to the twiddle factors
    index.
  */
  std::unique_ptr<FftWithPrecomputeBase> fft_precompute_;
  std::unique_ptr<FftWithPrecomputeBase> ifft_precompute_;
  FieldElement previous_coset_offset_;
};

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_LDE_CACHED_LDE_MANAGER_H_
