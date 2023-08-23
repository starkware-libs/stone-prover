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

#ifndef STARKWARE_FRI_FRI_COMMITTED_LAYER_H_
#define STARKWARE_FRI_FRI_COMMITTED_LAYER_H_

#include <memory>
#include <utility>
#include <vector>

#include "starkware/channel/prover_channel.h"
#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/fri/fri_layer.h"
#include "starkware/fri/fri_parameters.h"

namespace starkware {

/*
  Base class for layers which are committed for the Frilayer which belongs to it.
  FriCommittedLayer is already committed at construction and ready for decommitment.
*/
class FriCommittedLayer {
 public:
  explicit FriCommittedLayer(size_t fri_step) : fri_step_(fri_step) {}
  virtual ~FriCommittedLayer() = default;
  virtual void Decommit(const std::vector<uint64_t>& queries) = 0;

 protected:
  const size_t fri_step_;
};

using FirstLayerCallback = std::function<void(const std::vector<uint64_t>& queries)>;

/*
  This layer decommits using a callback function (It does not keep a FriLayer).
  It is mostly used for witness layer (FRI first layer).
*/
class FriCommittedLayerByCallback : public FriCommittedLayer {
 public:
  FriCommittedLayerByCallback(
      size_t fri_step, MaybeOwnedPtr<FirstLayerCallback> layer_queries_callback)
      : FriCommittedLayer(fri_step), layer_queries_callback_(std::move(layer_queries_callback)) {}

  void Decommit(const std::vector<uint64_t>& queries) override;

 private:
  MaybeOwnedPtr<FirstLayerCallback> layer_queries_callback_;
};

/*
  Commits on a FriLayer using a TableProver.
*/
class FriCommittedLayerByTableProver : public FriCommittedLayer {
 public:
  FriCommittedLayerByTableProver(
      size_t fri_step, MaybeOwnedPtr<const FriLayer> layer,
      const TableProverFactory& table_prover_factory, const FriParameters& params,
      size_t layer_num);

  void Decommit(const std::vector<uint64_t>& queries) override;

 private:
  void Commit();
  struct ElementsData {
    std::vector<ConstFieldElementSpan> elements;
    std::vector<FieldElementVector> raw_data;
  };

  ElementsData EvalAtPoints(const gsl::span<uint64_t>& required_row_indices);

  MaybeOwnedPtr<const FriLayer> fri_layer_;
  const FriParameters& params_;
  const size_t layer_num_;
  std::unique_ptr<TableProver> table_prover_;
};

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_COMMITTED_LAYER_H_
