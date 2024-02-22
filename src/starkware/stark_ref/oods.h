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

#ifndef STARKWARE_STARK_OODS_H_
#define STARKWARE_STARK_OODS_H_

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/algebra/fields/field_operations_helper.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/stark/composition_oracle.h"

namespace starkware {
namespace oods {

/*
  Given an evaluation of a composition polynomial of size and degree bound n_breaks * trace_length,
  breaks into n_breaks evaluations of polynomials of degree trace_length. Each evaluation is over a
  new domain. Returns the evaluations and the new domain. See breaker.h for more details.
*/
std::pair<Trace, std::unique_ptr<FftBases>> BreakCompositionPolynomial(
    const ConstFieldElementSpan& composition_evaluation, size_t n_breaks, const FftBases& bases);

/*
  Returns an AIR representing the given boundary constraints.
*/
std::unique_ptr<Air> CreateBoundaryAir(
    const Field& field, uint64_t trace_length, size_t n_columns,
    std::vector<std::tuple<size_t, FieldElement, FieldElement>>&& boundary_constraints);

/*
  Receives a random point from the verifier, z, and sends to the verifier the necessary values it
  needs for OODS:
  * The mask of the original traces at point z.
  * The broken trace at point z (broken from the composition polynomial evaluation of the original
  oracle).
  Returns the boundary constraints needed for the rest of the proof.
*/
std::vector<std::tuple<size_t, FieldElement, FieldElement>> ProveOods(
    ProverChannel* channel, const CompositionOracleProver& original_oracle,
    const CommittedTraceProverBase& broken_trace, bool use_extension_field);

/*
  Sends a random point to the prover, z, and receives from the prover the necessary values we need
  for OODS:
  * The mask of the original traces at point z.
  * The broken trace at point z (broken from the composition polynomial evaluation of the original
  oracle).
  Checks that applying the composition polynomial on the mask values equals the expected composition
  polynomial value, assembled from the broken trace values. Returns the boundary constraints needed
  for the rest of the proof.
*/
std::vector<std::tuple<size_t, FieldElement, FieldElement>> VerifyOods(
    const ListOfCosets& evaluation_domain, VerifierChannel* channel,
    const CompositionOracleVerifier& original_oracle, const FftBases& composition_eval_bases,
    bool use_extension_field);

}  // namespace oods
}  // namespace starkware

#endif  // STARKWARE_STARK_OODS_H_
