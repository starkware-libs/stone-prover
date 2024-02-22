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

#ifndef STARKWARE_STARK_STARK_H_
#define STARKWARE_STARK_STARK_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/trace.h"
#include "starkware/air/trace_context.h"
#include "starkware/algebra/domains/list_of_cosets.h"
#include "starkware/algebra/lde/cached_lde_manager.h"
#include "starkware/algebra/lde/lde.h"
#include "starkware/channel/prover_channel.h"
#include "starkware/channel/verifier_channel.h"
#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/fri/fri_parameters.h"
#include "starkware/stark/committed_trace.h"
#include "starkware/stark/composition_oracle.h"
#include "starkware/utils/json.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {

struct StarkParameters {
  StarkParameters(
      const Field& field, bool use_extension_field, size_t n_evaluation_domain_cosets,
      size_t trace_length, MaybeOwnedPtr<const Air> air, MaybeOwnedPtr<FriParameters> fri_params);

  size_t TraceLength() const { return Pow2(evaluation_domain.Bases().NumLayers()); }
  FieldElement TraceCosetOffset() const { return field.One(); }
  size_t NumCosets() const { return evaluation_domain.NumCosets(); }
  size_t NumColumns() const { return (air->NumColumns()); }

  static StarkParameters FromJson(
      const JsonValue& json, const Field& field, MaybeOwnedPtr<const Air> air,
      bool use_extension_field);

 private:
  /*
    Verifies that evaluation_domain and composition_eval_bases are compatible in the following
    sense: composition_eval_bases should contain the same elements as in the first D cosets (in bit
    reverse order) of evaluation_domain, where D is the degree bound of the air contraints. This is
    the domain on which we evaluate the composition polynomial.
  */
  void VerifyCompatibleDomains();

 public:
  Field field;
  bool use_extension_field;
  ListOfCosets evaluation_domain;

  MaybeOwnedPtr<const Air> air;
  MaybeOwnedPtr<FftBases> composition_eval_bases;
  MaybeOwnedPtr<FriParameters> fri_params;
};

struct StarkProverConfig {
  CachedLdeManager::Config cached_lde_config;

  /*
    Controls The number of tasks used to commit to a segment in the table prover.
  */
  size_t table_prover_n_tasks_per_segment;

  /*
    Evaluation of composition polynomial on the coset is split to different tasks of size
    constraint_polynomial_task_size each to allow multithreading.
    The larger the task size the lower the amortized threading overhead but this can also affect the
    fragmentation effect in case the number of tasks doesn't divide the coset by a multiple of the
    number of threads in the thread pool.
  */
  uint64_t constraint_polynomial_task_size;
  /*
    Number of merkle layer that are not stored in memory but instead recalculated when required by
    decommitment request. When n_out_of_memory_merkle_layers is 0 it means that all the data is
    stored in the merkle tree. When n_out_of_memory_merkle_layers is 1 it means that the layer of
    the leaves are not stored in memory and so on.
  */
  size_t n_out_of_memory_merkle_layers;

  FriProverConfig fri_prover_config;

  static StarkProverConfig InRam() {
    return {
        {
            /*store_full_lde=*/true,
            /*use_fft_for_eval=*/false,
        },
        /*table_prover_n_tasks_per_segment=*/32,
        /*constraint_polynomial_task_size=*/256,
        /*n_out_of_memory_merkle_layers=*/1,
        {
            /*max_non_chunked_layer_size=*/FriProverConfig::kDefaultMaxNonChunkedLayerSize,
            /*n_chunks_between_layers=*/FriProverConfig::kDefaultNumberOfChunksBetweenLayers,
            /*log_n_max_in_memory_fri_layer_elements=*/FriProverConfig::kAllInMemoryLayers,
        }};
  }

  static StarkProverConfig FromJson(const JsonValue& json);
};

class StarkProver {
 public:
  StarkProver(
      MaybeOwnedPtr<ProverChannel> channel, MaybeOwnedPtr<TableProverFactory> table_prover_factory,
      MaybeOwnedPtr<const StarkParameters> params, MaybeOwnedPtr<const StarkProverConfig> config)
      : channel_(std::move(channel)),
        table_prover_factory_(std::move(table_prover_factory)),
        params_(std::move(params)),
        config_(std::move(config)) {}

  /*
    Implements the STARK prover side of the protocol given a trace context which stores parameters
    for Trace generation, a constraint system defined in an AIR (to verify the Trace) and a set of
    protocol parameters definition attributes such as the required soundness. The prover uses a
    commitment scheme factory for generating commitments and decommitments and a prover channel for
    sending proof elements and receiving required randomness from the verifier (or simulated
    verifier in a non-interactive implementation). The STARK prover uses a Low Degree Test (LDT)
    prover as its main proof engine. We use FRI. The STARK prover is field and channel type
    agnostic.
    Note that in case there is an interaction, ProveStark creates a new Air with interaction
    elements, which is destroyed when the function ends.


    The main STARK prover steps are:
    - Generate the trace.
    - Perform a Low Degree Extension (LDE) on each column in the trace.
    - Feed a block commitment layer the extended trace to generate a commitment.
    - Get a set of random coefficients from the verifier required by the AIR in order to combine all
      sub-constraints into a single constraint.
    - Use the AIR constraint in order to calculate a virtual oracle (FRI top layer) representing the
      combination of the trace with the constraint.
    - Run a LDT prover (e.g. FRI) to prove that the virtual oracle is of the desired
      low degree.
    - The LDT (FRI) requires a method for responding to queries on the trace derived from queries on
      the virtual oracle. This is the last part that requires implementation by the STARK prover.
  */
  void ProveStark(std::unique_ptr<TraceContext> trace_context);

 private:
  void PerformLowDegreeTest(const CompositionOracleProver& oracle);

  /*
    Creates a CommittedTraceProver from a trace.
    Parameters:
    * trace: The trace to commit on.
    * bases: The domain on which the trace is defined.
    * bit_reverse: Whether the trace is saved in memory bit reversed with respect to bases.
  */
  CommittedTraceProver CommitOnTrace(
      Trace&& trace, const FftBases& bases, bool bit_reverse, const std::string& profiling_text);

  CompositionOracleProver OutOfDomainSamplingProve(CompositionOracleProver original_oracle);

  /*
    Validates that the given arguments regarding the first trace size are the same as in params_.
  */
  void ValidateFirstTraceSize(size_t n_rows, size_t n_columns);

  MaybeOwnedPtr<ProverChannel> channel_;
  MaybeOwnedPtr<TableProverFactory> table_prover_factory_;
  MaybeOwnedPtr<const StarkParameters> params_;
  MaybeOwnedPtr<const StarkProverConfig> config_;
};

class StarkVerifier {
 public:
  StarkVerifier(
      MaybeOwnedPtr<VerifierChannel> channel,
      MaybeOwnedPtr<const TableVerifierFactory> table_verifier_factory,
      MaybeOwnedPtr<const StarkParameters> params)
      : channel_(std::move(channel)),
        table_verifier_factory_(std::move(table_verifier_factory)),
        params_(std::move(params)) {}

  /*
    Implements the STARK verifier side of the protocol given constraint system defined in an AIR (to
    verify the Trace) and a set of protocol parameters definition attributes such as the required
    soundness. The verifier uses a commitment scheme factory for requesting commitments and
    decommitments and a verifier channel for receiving proof elements and possibly some hiding
    randomness from the prover (or simulated prover in a non-interactive implementation). The
    STARK verifier uses a Low Degree Test (LDT) verifier as its main verification engine. We use
    FRI. The STARK verifier is field and channel type agnostic.

    The main STARK verfier steps are:
    - Receive a commitment on the trace generated by the prover.
    - Send a set of random coefficients to the prover required by the AIR in order to combine all
      sub-constraints into a single constraint.
    - Calculate the AIR constraint in order to calculate points of a virtual oracle (FRI top layer)
      representing the combination of the trace with the constraint.
    - Run a LDT verfier (e.g. FRI) to verify that the virtual oracle is of the desired
      low degree.
    - The LDT (FRI) requires a method for calculating responses to queries on
      the virtual oracle based on responses sent by the prover that were derived from the trace.
      This is the last part that requires implementation by the STARK verifier.

    In case verification fails, an exception is thrown.
  */
  void VerifyStark();

  void SetSkipAssertForExtensionFieldTest() { skip_assert_for_extension_field_test_ = true; }

 private:
  /*
    Reads the trace's commitment.
    should_verify_base_field is relevant when using an extension field. It indicates that the
    verifier should verify that the field element queries of the trace are in the base field.
  */
  CommittedTraceVerifier ReadTraceCommitment(
      size_t n_columns, bool should_verify_base_field = false);

  void PerformLowDegreeTest(const CompositionOracleVerifier& oracle);

  CompositionOracleVerifier OutOfDomainSamplingVerify(CompositionOracleVerifier original_oracle);

  MaybeOwnedPtr<VerifierChannel> channel_;
  MaybeOwnedPtr<const TableVerifierFactory> table_verifier_factory_;
  MaybeOwnedPtr<const StarkParameters> params_;

  std::unique_ptr<CompositionPolynomial> composition_polynomial_;

  /*
    For tests only, relevant when using extension fields. When true, the verifier skips on checking
    that the response it gets on the LDE trace queries are field elements from the base field.
  */
  bool skip_assert_for_extension_field_test_ = false;
};

}  // namespace starkware

#endif  // STARKWARE_STARK_STARK_H_
