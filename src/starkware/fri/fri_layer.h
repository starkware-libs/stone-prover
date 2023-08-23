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

#ifndef STARKWARE_FRI_FRI_LAYER_H_
#define STARKWARE_FRI_FRI_LAYER_H_

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/algebra/fft/fft_with_precompute.h"
#include "starkware/fri/fri_folder.h"
#include "starkware/fri/fri_parameters.h"

namespace starkware {

class LdeManager;

/*
  Base class of all FRI layers.
  It contains the implementation of mutual behavior and data of all kinds of layers and interfaces
  for specific required applications.
  Layer evaluation is queried by chunks. The chunk size is a matter of internal implementation.
*/
class FriLayer {
 public:
  explicit FriLayer(MaybeOwnedPtr<const FftBases>&& domain)
      : domain_(std::move(domain)), layer_size_(domain_->At(0).Size()) {}

  virtual ~FriLayer() = default;
  FriLayer(FriLayer&&) = default;

  FriLayer(const FriLayer&) = delete;
  const FriLayer& operator=(const FriLayer&) = delete;
  const FriLayer& operator=(FriLayer&&) = delete;

  // The size of the whole layer (regardles implementation).
  uint64_t LayerSize() const { return layer_size_; }

  struct Storage {
    virtual ~Storage() = default;
  };

  // Create a helper storage to be used with method GetChunk.
  virtual std::unique_ptr<Storage> MakeStorage() const = 0;

  virtual uint64_t ChunkSize() const = 0;
  virtual ConstFieldElementSpan GetChunk(
      Storage* storage, size_t requested_size, size_t chunk_index) const = 0;
  virtual void GetChunk(
      Storage* storage, const FieldElementSpan& output, size_t requested_size,
      size_t chunk_index) const = 0;

  // Get evaluation at specific indices.
  virtual FieldElementVector EvalAtPoints(const gsl::span<uint64_t>& required_indices) const = 0;

  const FftBases* GetDomain() const { return domain_.get(); }

  // Get all the  evaluation of current layer as a vector.
  FieldElementVector GetAllEvaluation() const;

 protected:
  FieldElementVector MakeFieldElementVector(uint64_t size = 0) const {
    return FieldElementVector::MakeUninitialized(domain_->GetField(), size);
  }

  static MaybeOwnedPtr<const FftBases> CloneDomain(const FftBases* domain) {
    return TakeOwnershipFrom(domain->FromLayerAsUniquePtr(0));
  }

  static std::tuple<std::unique_ptr<FftBases>, std::vector<FieldElement>> SplitToCosets(
      const FftBases* domain, size_t chunk_size);

 private:
  MaybeOwnedPtr<const FftBases> domain_;

 protected:
  const uint64_t layer_size_;  // Caching the layer size for loop optimization.
};

/*
  A layer which keeps all the evaluation.
*/
class FriLayerInMemory : public FriLayer {
 public:
  explicit FriLayerInMemory(MaybeOwnedPtr<const FriLayer> prev_layer)
      : FriLayer(CloneDomain(prev_layer->GetDomain())),
        evaluation_(prev_layer->GetAllEvaluation()) {}
  explicit FriLayerInMemory(FieldElementVector evaluation, MaybeOwnedPtr<const FftBases> domain)
      : FriLayer(std::move(domain)), evaluation_(std::move(evaluation)) {}

  uint64_t ChunkSize() const override { return layer_size_; }
  ConstFieldElementSpan GetChunk(
      Storage* storage, size_t requested_size, size_t chunk_index) const override;
  void GetChunk(
      Storage* storage, const FieldElementSpan& output, size_t requested_size,
      size_t chunk_index) const override;

  FieldElementVector EvalAtPoints(const gsl::span<uint64_t>& required_indices) const override;

  std::unique_ptr<Storage> MakeStorage() const override { return std::make_unique<Storage>(); }

 private:
  FieldElementVector evaluation_;
};

/*
  A layer which keeps only the lde (and use it in order to calculate the evaluation).
*/
class FriLayerOutOfMemory : public FriLayer {
 public:
  FriLayerOutOfMemory(MaybeOwnedPtr<const FriLayer> prev_layer, size_t coset_size);
  FriLayerOutOfMemory(FieldElementVector&& evaluation, MaybeOwnedPtr<const FftBases> domain);

  uint64_t ChunkSize() const override { return coset_size_; }
  ConstFieldElementSpan GetChunk(
      Storage* storage, size_t requested_size, size_t chunk_index) const override;
  void GetChunk(
      Storage* storage, const FieldElementSpan& output, size_t requested_size,
      size_t chunk_index) const override;

  // Get evaluation at specific indices (of the layer evaluation).
  FieldElementVector EvalAtPoints(const gsl::span<uint64_t>& required_indices) const override;

  std::unique_ptr<Storage> MakeStorage() const override;

 private:
  struct OutOfMemoryStorage : public Storage {
    std::optional<FieldElementVector> accumulation;
    std::unique_ptr<FftWithPrecomputeBase> precomputed_fft;
  };

  void Build();                 // Build coset_offsets_.
  void InitLdeManager() const;  // Build lde_manager_.
  FftWithPrecomputeBase* GetPrecomputedFft(Storage* storage, size_t chunk_index) const;

  std::vector<FieldElement> coset_offsets_;
  size_t coset_size_;
  std::unique_ptr<FftBases> coset_bases_;

  // For caching reasons we make them mutable. They all could be initialized in the constructor.
  mutable std::unique_ptr<LdeManager> lde_manager_;  // Needed on get the non-first chunk.
  mutable FieldElementVector evaluation_;     // Can be used as first coset and to initialize LDE.
  mutable bool is_evaluation_moved_ = false;  // Evaluation cannot use for 1st chunk after LDE init.
};

/*
  A layer which is used as a proxy between layers. The proxy is the only kind of layer which folds
  the domain, so it must be at least one proxy between every two other types of layers.
*/
class FriLayerProxy : public FriLayer {
 public:
  using FriFolderBase = starkware::fri::details::FriFolderBase;

  FriLayerProxy(
      const FriFolderBase& folder, MaybeOwnedPtr<const FriLayer> prev_layer,
      FieldElement eval_point, const FriProverConfig* fri_prover_config);

  uint64_t ChunkSize() const override { return chunk_size_; }
  ConstFieldElementSpan GetChunk(
      Storage* storage, size_t requested_size, size_t chunk_index) const override;
  void GetChunk(
      Storage* storage, const FieldElementSpan& output, size_t requested_size,
      size_t chunk_index) const override;

  FieldElementVector EvalAtPoints(const gsl::span<uint64_t>& required_indices) const override;

  std::unique_ptr<Storage> MakeStorage() const override;

 private:
  /*
    NOTE(Ronny): Today there is no case of in-memory-layer -> proxy -> out-of-memory layer.
    Only out-of-memory-layer -> proxy -> out-of-memory layer, in-memory-layer -> out-of-memory layer
    or in-memory-layer -> proxy -> in-memory layer. That's why GetChunk is called only once between
    layers. In case there will be in-memory-layer -> proxy -> out-of-memory layer, we should
    consider to add prev_storage to ProxyStorage.
  */
  struct ProxyStorage : public Storage {
    ProxyStorage(const Field& field, uint64_t size)
        : accumulation(FieldElementVector::MakeUninitialized(field, size)) {}

    FieldElementVector accumulation;
  };

  MaybeOwnedPtr<const FftBases> FoldDomain(const FftBases* domain) {
    return TakeOwnershipFrom(domain->FromLayerAsUniquePtr(1));
  }

  uint64_t CalculateChunkSize();

  const FriFolderBase& folder_;
  MaybeOwnedPtr<const FriLayer> prev_layer_;
  const FieldElement eval_point_;
  const FriProverConfig* fri_prover_config_;
  std::unique_ptr<FftBases> coset_bases_;
  std::vector<FieldElement> coset_offsets_;
  uint64_t chunk_size_;
};

////////////////////////////////////////////////////////////////////////////

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_LAYER_H_
