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

#ifndef STARKWARE_FRI_FRI_VERIFIER_H_
#define STARKWARE_FRI_FRI_VERIFIER_H_

#include <memory>
#include <vector>

#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/fri/fri_details.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

class FriVerifier {
 public:
  using FirstLayerCallback =
      std::function<FieldElementVector(const std::vector<uint64_t>& queries)>;

  FriVerifier(
      MaybeOwnedPtr<VerifierChannel> channel,
      MaybeOwnedPtr<const TableVerifierFactory> table_verifier_factory,
      MaybeOwnedPtr<const FriParameters> params,
      MaybeOwnedPtr<FirstLayerCallback> first_layer_queries_callback)
      : channel_(UseOwned(channel)),
        table_verifier_factory_(UseOwned(table_verifier_factory)),
        params_(UseOwned(params)),
        folder_(fri::details::FriFolderFromField(params_->field)),
        first_layer_queries_callback_(UseOwned(first_layer_queries_callback)),
        n_layers_(params_->fri_step_list.size()) {}

  /*
    Applies the FRI protocol to verify that the prover has access to a witness whose degree is
    smaller than 2^n_layers. first_layer_queries_callback is a callback function that will be called
    once, with the indices of the queries for the first layer. Given a vector of query indices, this
    callback is responsible for:
      1. Given the specific indices in the first layer - it returns their values to be used by
         VerifFri().
      2. Verifying the correctness of the returned values against the prover's decommitments.
    In case the verification fails, it throws an exception.
  */
  void VerifyFri();

 private:
  /*
    Initialize local class variables.
  */
  void Init();

  /*
    For each layer - we send a random field element, and obtain a commitment. Both the evaluation
    points, and the table_verifiers used to read the commitments, are returned via the output
    parameters.
  */
  void CommitmentPhase();

  /*
    Reads the coefficients of the interpolation polynomial of the last layer and computes
    expected_last_layer_.
  */
  void ReadLastLayerCoefficients();

  void VerifyFirstLayer();

  /*
    For each of the inner layers (i.e. not the first nor the last), we send the queries through its
    appropriate "channel", and authenticate responses. We go layer-by-layer, resolving all queries
    in parallel. If all is correct - the last layer (not computed in this function) is expected to
    agree with expected_last_layer_. If a verification on some layer fails, it throws an exception.
  */
  void VerifyInnerLayers();

  /*
    Verifies that the elements of the last layer are consistent with expected_last_layer_.
    Throws an exception otherwise.
  */
  void VerifyLastLayer();

  MaybeOwnedPtr<VerifierChannel> channel_;
  MaybeOwnedPtr<const TableVerifierFactory> table_verifier_factory_;
  MaybeOwnedPtr<const FriParameters> params_;
  std::unique_ptr<const fri::details::FriFolderBase> folder_;
  MaybeOwnedPtr<FirstLayerCallback> first_layer_queries_callback_;
  size_t n_layers_;

  std::optional<FieldElementVector> expected_last_layer_;
  std::optional<FieldElement> first_eval_point_;
  std::vector<FieldElement> eval_points_;
  std::vector<std::unique_ptr<TableVerifier>> table_verifiers_;
  std::vector<uint64_t> query_indices_;
  std::vector<FieldElement> query_results_;
};

}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_VERIFIER_H_
