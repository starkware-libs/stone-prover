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

#ifndef STARKWARE_FRI_FRI_PROVER_H_
#define STARKWARE_FRI_FRI_PROVER_H_

#include <functional>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "starkware/fri/fri_committed_layer.h"
#include "starkware/fri/fri_details.h"

namespace starkware {

class FriProver {
 public:
  using FirstLayerCallback = std::function<void(const std::vector<uint64_t>& queries)>;

  FriProver(
      MaybeOwnedPtr<ProverChannel> channel, MaybeOwnedPtr<TableProverFactory> table_prover_factory,
      MaybeOwnedPtr<FriParameters> params, FieldElementVector&& witness,
      MaybeOwnedPtr<FirstLayerCallback> first_layer_queries_callback,
      MaybeOwnedPtr<const FriProverConfig> fri_prover_config);

  /*
    Applies the FRI protocol to prove that the given witness is of degree smaller than 2^n_layers.
    first_layer_queries_callback is a callback function that will be called once, with the indices
    of the queries for the first layer. This callback is responsible for:
      1. Sending enough information to the verifier so that it will be able to compute the values at
      these indices.
      2. Sending the relevant decommitment.
  */
  void ProveFri();

 private:
  /*
    The commitment phase of the ProveFri sequence is assumed to already have the witness available
    to it as the first layer in the class's committed_layers_ variable, and use it to:
      1. Fill the rest of layers, receiving randomness over the channel to computer the next layer.
      2. Fill the table provers in the table_provers_ class variable, one for each layer, which are
         used to commit on layers, and later on (though not in this phase) authenticate commitments.
    The function returns the last FriLayer.
  */
  MaybeOwnedPtr<const FriLayer> CommitmentPhase();

  /*
    Sends the coefficients of the polynomial, f(x), of the last FRI layer (which is expected to be
    of degree < last_layer_degree_bound) over the channel. Note: the coefficients are computed
    without taking the basis offset into account (that is, the first element of the last layer will
    always be f(1)).
  */
  void SendLastLayer(MaybeOwnedPtr<const FriLayer>&& last_layer);

  /*
    Creates the next fri layer. Next fri layer is always FriLayerProxy. It is created between every
    two real layers (in or out of memory). Skipping layers is done by creating few FriLayerProxy
    between them.
  */
  MaybeOwnedPtr<const FriLayer> CreateNextFriLayer(
      MaybeOwnedPtr<const FriLayer> current_layer, size_t fri_step, size_t* basis_index);

  // Data:
  MaybeOwnedPtr<ProverChannel> channel_;
  MaybeOwnedPtr<TableProverFactory> table_prover_factory_;
  MaybeOwnedPtr<FriParameters> params_;
  std::unique_ptr<fri::details::FriFolderBase> folder_;
  FieldElementVector witness_;
  MaybeOwnedPtr<const FriProverConfig> fri_prover_config_;

  size_t n_layers_;
  std::vector<MaybeOwnedPtr<FriCommittedLayer>> committed_layers_;
};

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_PROVER_H_
