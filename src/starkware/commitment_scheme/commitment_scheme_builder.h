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

#ifndef STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_BUILDER_H_
#define STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_BUILDER_H_

#include <memory>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/prover_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/packaging_commitment_scheme.h"
#include "starkware/crypt_tools/invoke.h"

namespace starkware {

/*
  Creates a chain of commitment scheme layers that handle the commitment. Returns the outermost
  layer (which is a packaging commitment scheme prover layer).
*/
template <typename HashT>
PackagingCommitmentSchemeProver<HashT> MakeCommitmentSchemeProver(
    size_t size_of_element, size_t n_elements_in_segment, size_t n_segments, ProverChannel* channel,
    size_t n_verifier_friendly_commitment_layers, const CommitmentHashes& commitment_hashes,
    size_t n_out_of_memory_merkle_layers = 0);

template <typename HashT>
PackagingCommitmentSchemeVerifier<HashT> MakeCommitmentSchemeVerifier(
    size_t size_of_element, uint64_t n_elements, VerifierChannel* channel,
    size_t n_verifier_friendly_commitment_layers, const CommitmentHashes& commitment_hashes);

}  // namespace starkware

#include "starkware/commitment_scheme/commitment_scheme_builder.inl"

#endif  // STARKWARE_COMMITMENT_SCHEME_COMMITMENT_SCHEME_BUILDER_H_
