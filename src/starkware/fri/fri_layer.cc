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

#include "starkware/fri/fri_layer.h"

#include <algorithm>

#include "starkware/algebra/lde/lde.h"

namespace starkware {

//  Class FriLayer.

/*
  Get all the  evaluation of current layer as a vector.
  It is done by looping over the chunks of current layer.
*/
FieldElementVector FriLayer::GetAllEvaluation() const {
  size_t chunk_size = ChunkSize();
  FieldElementVector all_evaluation = MakeFieldElementVector(layer_size_);
  const FieldElementSpan whole_evaluation(all_evaluation);
  size_t chunks_count = SafeDiv(layer_size_, chunk_size);
  auto storage = MakeStorage();
  for (size_t chunk_index = 0; chunk_index < chunks_count; ++chunk_index) {
    const FieldElementSpan chunk = whole_evaluation.SubSpan(chunk_index * chunk_size, chunk_size);
    GetChunk(storage.get(), chunk, chunk_size, chunk_index);
  }
  return all_evaluation;
}

std::tuple<std::unique_ptr<FftBases>, std::vector<FieldElement>> FriLayer::SplitToCosets(
    const FftBases* domain, size_t chunk_size) {
  const size_t log_cosets = domain->At(0).BasisSize() - SafeLog2(chunk_size);
  return domain->SplitToCosets(log_cosets);
}

//  Class FriLayerInMemory.

ConstFieldElementSpan FriLayerInMemory::GetChunk(
    Storage* /* storage */, size_t requested_size, size_t chunk_index) const {
  ConstFieldElementSpan sp(evaluation_);
  return (requested_size < layer_size_) ? sp.SubSpan(chunk_index * requested_size, requested_size)
                                        : sp;
}

void FriLayerInMemory::GetChunk(
    Storage* /* storage */, const FieldElementSpan& output, size_t requested_size,
    size_t chunk_index) const {
  output.CopyDataFrom(
      ConstFieldElementSpan(evaluation_).SubSpan(chunk_index * requested_size, requested_size));
}

FieldElementVector FriLayerInMemory::EvalAtPoints(
    const gsl::span<uint64_t>& required_indices) const {
  FieldElementVector res = FieldElementVector::Make(GetDomain()->GetField());
  res.Reserve(required_indices.size());
  for (uint64_t i : required_indices) {
    res.PushBack(evaluation_[i]);
  }
  return res;
}

//  Class FriLayerOutOfMemory.

FriLayerOutOfMemory::FriLayerOutOfMemory(
    MaybeOwnedPtr<const FriLayer> prev_layer, size_t coset_size)
    : FriLayer(CloneDomain(prev_layer->GetDomain())),
      coset_size_(coset_size),
      evaluation_(MakeFieldElementVector(coset_size)) {
  ASSERT_RELEASE(coset_size_ <= layer_size_, "layer_prefix must be shorter than the domain");

  prev_layer->GetChunk(nullptr, evaluation_, ChunkSize(), 0);
  Build();
}

FriLayerOutOfMemory::FriLayerOutOfMemory(
    FieldElementVector&& evaluation, MaybeOwnedPtr<const FftBases> domain)
    : FriLayer(std::move(domain)),
      coset_size_(evaluation.Size()),
      evaluation_(std::move(evaluation)) {
  Build();
}

void FriLayerOutOfMemory::Build() {
  // Split to cosets.
  std::tie(coset_bases_, coset_offsets_) = SplitToCosets(GetDomain(), ChunkSize());
}

// Lazy initialize of the LDE manager. Call this function before using lde_manager_.
void FriLayerOutOfMemory::InitLdeManager() const {
  if (lde_manager_) {
    return;
  }
  const auto first_bases = coset_bases_->GetShiftedBasesAsUniquePtr(coset_offsets_[0]);
  lde_manager_ = MakeLdeManager(*first_bases);
  lde_manager_->AddEvaluation(std::move(evaluation_));
  is_evaluation_moved_ = true;
}

ConstFieldElementSpan FriLayerOutOfMemory::GetChunk(
    Storage* storage, size_t requested_size, size_t chunk_index) const {
  ASSERT_RELEASE(
      storage != nullptr && requested_size <= coset_size_ &&
          chunk_index < SafeDiv(layer_size_, coset_size_),
      "Bad parameters for FriLayerOutOfMemory::GetChunk");

  if (chunk_index == 0 && !is_evaluation_moved_) {
    return evaluation_;
  }
  InitLdeManager();
  OutOfMemoryStorage* out_of_mem_storage = dynamic_cast<OutOfMemoryStorage*>(storage);
  ASSERT_RELEASE(out_of_mem_storage != nullptr, "No storage");
  auto& accumulation = out_of_mem_storage->accumulation;
  if (!accumulation.has_value()) {
    accumulation = FieldElementVector::MakeUninitialized(GetDomain()->GetField(), ChunkSize());
  }
  lde_manager_->EvalOnCoset(
      coset_offsets_[chunk_index], std::vector<FieldElementSpan>{accumulation->AsSpan()});
  const FieldElementVector& const_storage = *out_of_mem_storage->accumulation;
  return const_storage.AsSpan().SubSpan(0, requested_size);
}

void FriLayerOutOfMemory::GetChunk(
    Storage* storage, const FieldElementSpan& output, size_t requested_size,
    size_t chunk_index) const {
  ASSERT_RELEASE(
      requested_size <= coset_size_ && chunk_index < SafeDiv(layer_size_, coset_size_),
      "Bad parameters for FriLayerOutOfMemory::GetChunk");

  // If the evaluation exists and first chunk was requested:
  if (chunk_index == 0 && !is_evaluation_moved_) {
    output.CopyDataFrom(ConstFieldElementSpan(evaluation_).SubSpan(0, requested_size));
    return;
  }

  // The rest of the chunks:
  FftWithPrecomputeBase* precompute = GetPrecomputedFft(storage, chunk_index);
  lde_manager_->EvalOnCoset(
      coset_offsets_[chunk_index], std::vector<FieldElementSpan>{output}, precompute);
}

FftWithPrecomputeBase* FriLayerOutOfMemory::GetPrecomputedFft(
    Storage* storage, size_t chunk_index) const {
  OutOfMemoryStorage* out_of_mem_storage = dynamic_cast<OutOfMemoryStorage*>(storage);
  std::unique_ptr<FftWithPrecomputeBase>& precompute = out_of_mem_storage->precomputed_fft;
  // Lazy initialize of the FFT precompute.
  if (precompute == nullptr) {
    InitLdeManager();
    precompute = lde_manager_->FftPrecompute(coset_offsets_[0]);
  }
  if (chunk_index > 0) {
    precompute->ShiftTwiddleFactors(coset_offsets_[chunk_index], coset_offsets_[chunk_index - 1]);
  }
  return precompute.get();
}

FieldElementVector FriLayerOutOfMemory::EvalAtPoints(
    const gsl::span<uint64_t>& required_indices) const {
  FieldElementVector res =
      FieldElementVector::MakeUninitialized(GetDomain()->GetField(), required_indices.size());

  FieldElementVector points = FieldElementVector::Make(GetDomain()->GetField());
  points.Reserve(required_indices.size());
  const auto& domain = GetDomain()->At(0);
  for (auto index : required_indices) {
    points.PushBack(domain.GetFieldElementAt(index));
  }

  InitLdeManager();
  lde_manager_->EvalAtPoints(0, points, res);

  return res;
}

std::unique_ptr<FriLayer::Storage> FriLayerOutOfMemory::MakeStorage() const {
  Field field = GetDomain()->GetField();
  return std::make_unique<OutOfMemoryStorage>();
}

//  Class FriLayerProxy.

FriLayerProxy::FriLayerProxy(
    const FriFolderBase& folder, MaybeOwnedPtr<const FriLayer> prev_layer, FieldElement eval_point,
    const FriProverConfig* fri_prover_config)
    : FriLayer(FoldDomain(prev_layer->GetDomain())),
      folder_(folder),
      prev_layer_(std::move(prev_layer)),
      eval_point_(std::move(eval_point)),
      fri_prover_config_(fri_prover_config),
      chunk_size_(CalculateChunkSize()) {
  std::tie(coset_bases_, coset_offsets_) = SplitToCosets(prev_layer_->GetDomain(), ChunkSize() * 2);
}

/*
  The chunk-size of the current layer is half the size of the previous layer, unless the layer is
  big and not already divided into chunks. The layer is considered as too big if it is bigger than
  max_non_chunked_layer_size.
  Big non-chunked layers are divided into n_chunks_between_layers chunks.
*/
uint64_t FriLayerProxy::CalculateChunkSize() {
  uint64_t prev_layer_size = prev_layer_->LayerSize();
  uint64_t prev_layer_chunk_size = prev_layer_->ChunkSize();
  bool not_split = prev_layer_chunk_size == prev_layer_size;
  return (not_split && prev_layer_size > fri_prover_config_->max_non_chunked_layer_size)
             ? std::max(
                   fri_prover_config_->max_non_chunked_layer_size,
                   SafeDiv(prev_layer_size, fri_prover_config_->n_chunks_between_layers))
             : SafeDiv(prev_layer_chunk_size, 2);
}

ConstFieldElementSpan FriLayerProxy::GetChunk(
    Storage* storage, size_t requested_size, size_t chunk_index) const {
  const auto chunk_domain = coset_bases_->GetShiftedBasesAsUniquePtr(coset_offsets_[chunk_index]);
  ASSERT_DEBUG(requested_size == ChunkSize(), "requested_size is different than ChunkSize()");

  ProxyStorage* proxy_storage = dynamic_cast<ProxyStorage*>(storage);

  auto prev_storage = prev_layer_->MakeStorage();
  folder_.ComputeNextFriLayer(
      chunk_domain->At(0),
      prev_layer_->GetChunk(prev_storage.get(), requested_size * 2, chunk_index), eval_point_,
      proxy_storage->accumulation);
  return proxy_storage->accumulation;
}

void FriLayerProxy::GetChunk(
    Storage* /* storage */, const FieldElementSpan& output, size_t requested_size,
    size_t chunk_index) const {
  const auto chunk_domain = coset_bases_->GetShiftedBasesAsUniquePtr(coset_offsets_[chunk_index]);

  auto prev_storage = prev_layer_->MakeStorage();
  ASSERT_DEBUG(requested_size == ChunkSize(), "requested_size is bigger than ChunkSize()");
  auto chunk = prev_layer_->GetChunk(prev_storage.get(), requested_size * 2, chunk_index);
  folder_.ComputeNextFriLayer(chunk_domain->At(0), chunk, eval_point_, output);
}

FieldElementVector FriLayerProxy::EvalAtPoints(
    const gsl::span<uint64_t>& /* required_indices */) const {
  ASSERT_RELEASE(false, "Should never be called");
  return MakeFieldElementVector();
}

std::unique_ptr<FriLayer::Storage> FriLayerProxy::MakeStorage() const {
  Field field = GetDomain()->GetField();
  return std::make_unique<ProxyStorage>(field, ChunkSize());
}

}  // namespace starkware
