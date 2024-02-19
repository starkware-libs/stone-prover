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

#include "starkware/algebra/lde/cached_lde_manager.h"

#include <map>

namespace starkware {

std::unique_ptr<CachedLdeManager::LdeCacheEntry> CachedLdeManager::AllocateStorage() const {
  if (config_.store_full_lde) {
    return nullptr;
  }
  return std::make_unique<LdeCacheEntry>(InitializeEntry());
}

const CachedLdeManager::LdeCacheEntry* CachedLdeManager::EvalOnCoset(
    uint64_t coset_index, LdeCacheEntry* storage) {
  ASSERT_RELEASE(done_adding_, "Must call FinalizeAdding() before calling EvalOnCoset()");
  ASSERT_RELEASE(coset_index < coset_offsets_->Size(), "Coset index out of bounds.");

  if (cache_[coset_index].has_value()) {
    return &*cache_[coset_index];
  }

  const FieldElement& coset_offset = coset_offsets_->At(coset_index);
  if (coset_offset != previous_coset_offset_) {
    fft_precompute_->ShiftTwiddleFactors(coset_offset, previous_coset_offset_);
    previous_coset_offset_ = coset_offset;
  }

  if (config_.store_full_lde) {
    cache_[coset_index] = InitializeEntry();
    storage = &*cache_[coset_index];
  }

  ASSERT_RELEASE(storage != nullptr, "Invalid storage");
  ASSERT_RELEASE(
      lde_manager_.HasValue(), "Cannot evaluate new values after FinalizeEvaluations() was called");

  // Evaluate on columns.
  lde_manager_->EvalOnCoset(
      coset_offset, std::vector<FieldElementSpan>(storage->begin(), storage->end()),
      fft_precompute_.get());

  // Cache if needed.
  return storage;
}

void CachedLdeManager::EvalAtPoints(
    gsl::span<const std::pair<uint64_t, uint64_t>> coset_and_point_indices,
    gsl::span<const FieldElementSpan> outputs) {
  ASSERT_RELEASE(done_adding_, "Must call FinalizeAdding() before calling EvalAtPoints()");
  ASSERT_RELEASE(outputs.size() == n_columns_, "Wrong number of output columns");
  for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
    ASSERT_RELEASE(
        coset_and_point_indices.size() == outputs[column_index].Size(),
        "Number of output points is different than number of input points");
  }
  for (const auto& [coset_index, point_index] : coset_and_point_indices) {
    (void)coset_index;  // Unused.
    ASSERT_RELEASE(point_index < domain_size_, "Point index out of range.");
  }

  if (config_.store_full_lde) {
    // Look up coset in cache.
    for (size_t i = 0; i < coset_and_point_indices.size(); ++i) {
      const auto& [coset_index, point_index] = coset_and_point_indices[i];
      ASSERT_RELEASE(
          cache_[coset_index].has_value(),
          "EvalAtPoints with config_.store_full_lde requested a coset that is not cached!");
      for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
        outputs[column_index].Set(i, (*cache_[coset_index])[column_index].At(point_index));
      }
    }
    return;
  }

  // If not found, consider computing the entire coset.
  if (config_.use_fft_for_eval) {
    std::map<uint64_t, std::vector<size_t>> coset_to_query_index;
    for (size_t i = 0; i < coset_and_point_indices.size(); ++i) {
      const auto& [coset_index, point_index] = coset_and_point_indices[i];
      (void)point_index;  // Unused.
      coset_to_query_index[coset_index].push_back(i);
    }
    LdeCacheEntry entry = InitializeEntry();
    for (const auto& [coset_index, query_indices] : coset_to_query_index) {
      auto coset_evaluation = EvalOnCoset(coset_index, &entry);
      for (size_t query_index : query_indices) {
        for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
          const uint64_t point_index = coset_and_point_indices[query_index].second;
          outputs[column_index].Set(query_index, (*coset_evaluation)[column_index].At(point_index));
        }
      }
    }
    return;
  }

  // Evaluate pointwise.
  ASSERT_RELEASE(
      lde_manager_.HasValue(), "Cannot evaluate new values after FinalizeEvaluations() was called");
  FieldElementVector points = FieldElementVector::Make(coset_offsets_->At(0).GetField());
  points.Reserve(coset_and_point_indices.size());
  for (const auto& [coset_index, point_index] : coset_and_point_indices) {
    auto domain = lde_manager_->GetDomain(coset_offsets_->At(coset_index));
    points.PushBack(domain->GetFieldElementAt(point_index));
  }

  for (size_t column_index = 0; column_index < n_columns_; ++column_index) {
    EvalAtPointsNotCached(column_index, points, outputs[column_index]);
  }
}

void CachedLdeManager::EvalAtPointsNotCached(
    size_t column_index, const ConstFieldElementSpan& points, const FieldElementSpan& output) {
  ASSERT_RELEASE(
      lde_manager_.HasValue(), "Cannot evaluate new values after FinalizeEvaluations() was called");
  lde_manager_->EvalAtPoints(column_index, points, output);
}

CachedLdeManager::LdeCacheEntry CachedLdeManager::InitializeEntry() const {
  LdeCacheEntry entry;
  const uint64_t coset_size = domain_size_;
  entry.reserve(n_columns_);
  for (size_t i = 0; i < n_columns_; ++i) {
    entry.push_back(
        FieldElementVector::MakeUninitialized(coset_offsets_->At(0).GetField(), coset_size));
  }
  return entry;
}

void CachedLdeManager::FinalizeEvaluations() {
  ASSERT_RELEASE(done_adding_, "Must call FinalizeAdding() before calling FinalizeEvaluations()");
  if (config_.store_full_lde) {
    // This will make lde_manager_.get() == nullptr;
    lde_manager_.reset();
  }
}

}  // namespace starkware
