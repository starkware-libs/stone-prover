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

#include "starkware/stark/stark.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "starkware/algebra/fields/field_operations_helper.h"
#include "starkware/algebra/utils/invoke_template_version.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/fri/fri_prover.h"
#include "starkware/fri/fri_verifier.h"
#include "starkware/math/math.h"
#include "starkware/stark/oods.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/bit_reversal.h"
#include "starkware/utils/profiling.h"

namespace starkware {

namespace {

// ------------------------------------------------------------------------------------------
//  Prover and Verifier common code
// ------------------------------------------------------------------------------------------

/*
  Creates the composition polynomial using powers of one random element.
*/
std::unique_ptr<CompositionPolynomial> CreateCompositionPolynomial(
    Channel* channel, const Field& field, const FieldElement& trace_generator, const Air& air) {
  size_t num_random_coefficients_required = air.NumRandomCoefficients();
  FieldElementVector random_coefficients = FieldElementVector::Make(field);
  FieldElement alpha =
      channel->GetRandomFieldElementFromVerifier(field, "Constraint polynomial random element");
  FieldElement curr = field.One();
  for (size_t i = 0; i < num_random_coefficients_required; ++i) {
    random_coefficients.PushBack(curr);
    curr = curr * alpha;
  }
  return air.CreateCompositionPolynomial(trace_generator, random_coefficients);
}

/*
  Translates indices from fri_params->fft_bases to evaluation_domain. Returns a vector of pairs
  (coset_index, offset) s.t.
    fri_domain[fri_query] = evaluation_domain.GetFieldElementAt(coset_index, offset).
  Currently, FRI ignores the offset, so
  this equation is true only up to the offset correction between evaluation domain and
  fri_params->fft_bases.
*/
std::vector<std::pair<uint64_t, uint64_t>> FriQueriesToEvaluationDomainQueries(
    const std::vector<uint64_t>& fri_queries, const uint64_t trace_length) {
  std::vector<std::pair<uint64_t, uint64_t>> queries;
  queries.reserve(fri_queries.size());
  for (uint64_t fri_query : fri_queries) {
    const uint64_t coset_index = fri_query / trace_length;
    const uint64_t offset = fri_query & (trace_length - 1);
    queries.emplace_back(coset_index, offset);
  }
  return queries;
}

/*
  Computes the FRI degree bound from last_layer_degree_bound and fri_step_list.
*/
uint64_t GetFriExpectedDegreeBound(const FriParameters& fri_params) {
  uint64_t expected_bound = fri_params.last_layer_degree_bound;
  for (const size_t fri_step : fri_params.fri_step_list) {
    expected_bound *= Pow2(fri_step);
  }
  return expected_bound;
}

/*
  Get random elements from channel for interaction purpose.
*/
FieldElementVector GetInteractionElements(
    const size_t n_interaction_elements, const Field& field, Channel* channel) {
  auto interaction_elms_vec = FieldElementVector::Make(field);
  interaction_elms_vec.Reserve(n_interaction_elements);
  for (size_t i = 0; i < n_interaction_elements; ++i) {
    interaction_elms_vec.PushBack(channel->GetRandomFieldElementFromVerifier(
        field, "Interaction element #" + std::to_string(i)));
  }
  return interaction_elms_vec;
}

}  // namespace

// ------------------------------------------------------------------------------------------
//  StarkParameters
// ------------------------------------------------------------------------------------------

static std::unique_ptr<FftBases> GenerateCompositionBases(const Field& field, const Air& air) {
  const size_t log_size = SafeLog2(air.GetCompositionPolynomialDegreeBound());
  auto bases = InvokeFieldTemplateVersion(
      [&](auto field_tag) -> std::unique_ptr<FftBases> {
        using FieldElementT = typename decltype(field_tag)::type;
        FieldElementT offset = FieldElementT::GetBaseGenerator();

        return std::make_unique<
            MultiplicativeFftBases<FieldElementT, MultiplicativeGroupOrdering::kBitReversedOrder>>(
            log_size, offset);
      },
      field);

  return bases;
}

void StarkParameters::VerifyCompatibleDomains() {
  const auto all_offsets = evaluation_domain.CosetsOffsets();
  const auto n_relevant_cosets =
      SafeDiv(this->air->GetCompositionPolynomialDegreeBound(), TraceLength());
  const auto& [fft_elements, cosets] =
      composition_eval_bases->SplitToCosets(SafeLog2(n_relevant_cosets));

  // Verify offsets and cosets sizes.
  ASSERT_RELEASE(
      all_offsets.size() >= n_relevant_cosets, "Not enough offsets in evaluation domain.");
  ASSERT_RELEASE(
      cosets.size() == n_relevant_cosets, "Number of cosets in composition_eval_bases is wrong.");

  for (size_t i = 0; i < n_relevant_cosets; i++) {
    ASSERT_RELEASE(
        all_offsets[BitReverse(i, SafeLog2(all_offsets.size()))] == cosets[i],
        "offset of coset: " + std::to_string(i) + " is not compatible");
  }

  // Verify compatibility of the two groups.
  const auto& eval_domain_group = evaluation_domain.Group();
  const auto& fft_elements_group = fft_elements->At(0);
  ASSERT_RELEASE(
      eval_domain_group.Size() == fft_elements_group.Size(), "Groups have difference sizes.");
  ASSERT_RELEASE(
      eval_domain_group.GetFieldElementAt(0) == fft_elements_group.GetFieldElementAt(0),
      "The first elements in the groups are not the same.");
  ASSERT_RELEASE(
      eval_domain_group.GetFieldElementAt(1) == fft_elements_group.GetFieldElementAt(1),
      "The second elements in the groups are not the same.");
}

StarkParameters::StarkParameters(
    const Field& field, const bool use_extension_field, size_t n_evaluation_domain_cosets,
    size_t trace_length, MaybeOwnedPtr<const Air> air, MaybeOwnedPtr<FriParameters> fri_params)
    : field(field),
      use_extension_field(use_extension_field),
      evaluation_domain(ListOfCosets::MakeListOfCosets(
          trace_length, n_evaluation_domain_cosets, field,
          MultiplicativeGroupOrdering::kBitReversedOrder)),
      air(std::move(air)),
      composition_eval_bases(TakeOwnershipFrom(GenerateCompositionBases(field, *this->air))),
      fri_params(std::move(fri_params)) {
  ASSERT_RELEASE(
      IsPowerOfTwo(n_evaluation_domain_cosets), "The number of cosets must be a power of 2.");
  if (use_extension_field) {
    ASSERT_RELEASE(
        IsExtensionField(field),
        "Use extension field is true but the field is not of type extension field.");
  }

  // Check that the fri_step_list and last_layer_degree_bound parameters are consistent with the
  // trace length. This is the expected degree in out of domain sampling.
  const uint64_t expected_fri_degree_bound = GetFriExpectedDegreeBound(*this->fri_params);
  const uint64_t stark_degree_bound = trace_length;
  ASSERT_RELEASE(
      expected_fri_degree_bound == stark_degree_bound,
      "Fri parameters do not match stark degree bound. Expected FRI degree from "
      "FriParameters: " +
          std::to_string(expected_fri_degree_bound) +
          ". STARK: " + std::to_string(stark_degree_bound));

  VerifyCompatibleDomains();
}

StarkParameters StarkParameters::FromJson(
    const JsonValue& json, const Field& field, MaybeOwnedPtr<const Air> air,
    const bool use_extension_field) {
  const uint64_t trace_length = air->TraceLength();
  const size_t log_trace_length = SafeLog2(trace_length);
  const size_t log_n_cosets = json["log_n_cosets"].AsSizeT();
  const size_t n_cosets = Pow2(log_n_cosets);

  auto bases = InvokeFieldTemplateVersion(
      [&](auto field_tag) -> std::unique_ptr<FftBases> {
        using FieldElementT = typename decltype(field_tag)::type;
        return std::make_unique<FftBasesDefaultImpl<FieldElementT>>(
            MakeFftBases(log_trace_length + log_n_cosets, FieldElementT::One()));
      },
      field);

  FriParameters fri_params =
      FriParameters::FromJson(json["fri"], TakeOwnershipFrom(std::move(bases)), field);

  return StarkParameters(
      field, use_extension_field, n_cosets, trace_length, std::move(air),
      UseMovedValue(std::move(fri_params)));
}

// ------------------------------------------------------------------------------------------
//  StarkProverConfig
// ------------------------------------------------------------------------------------------

StarkProverConfig StarkProverConfig::FromJson(const JsonValue& json) {
  const bool store_full_lde = json["cached_lde_config"]["store_full_lde"].AsBool();
  const bool use_fft_for_eval = json["cached_lde_config"]["use_fft_for_eval"].AsBool();
  const uint64_t constraint_polynomial_task_size =
      json["constraint_polynomial_task_size"].AsUint64();
  const size_t table_prover_n_tasks_per_segment =
      json["table_prover_n_tasks_per_segment"].AsSizeT();
  const size_t n_out_of_memory_merkle_layers = json["n_out_of_memory_merkle_layers"].AsSizeT();

  const JsonValue fri_prover_config = json["fri_prover"];
  size_t max_non_chunked_layer_size = FriProverConfig::kDefaultMaxNonChunkedLayerSize;
  size_t n_chunks_between_layers = FriProverConfig::kDefaultNumberOfChunksBetweenLayers;
  size_t log_n_max_in_memory_fri_layer_elements = FriProverConfig::kAllInMemoryLayers;
  if (fri_prover_config.HasValue()) {
    const JsonValue max_layer_size = fri_prover_config["max_non_chunked_layer_size"];
    if (max_layer_size.HasValue()) {
      max_non_chunked_layer_size = max_layer_size.AsSizeT();
    }
    const JsonValue n_chunks = fri_prover_config["n_chunks_between_layers"];
    if (n_chunks.HasValue()) {
      n_chunks_between_layers = n_chunks.AsSizeT();
    }

    const JsonValue log_n_in_memory_fri =
        fri_prover_config["log_n_max_in_memory_fri_layer_elements"];
    if (log_n_in_memory_fri.HasValue()) {
      log_n_max_in_memory_fri_layer_elements = log_n_in_memory_fri.AsSizeT();
    }
  }

  return {
      {
          /*store_full_lde=*/store_full_lde,
          /*use_fft_for_eval=*/use_fft_for_eval,
      },
      /*table_prover_n_tasks_per_segment=*/table_prover_n_tasks_per_segment,
      /*constraint_polynomial_task_size=*/constraint_polynomial_task_size,
      /*n_out_of_memory_merkle_layers=*/n_out_of_memory_merkle_layers,
      {
          /*max_non_chunked_layer_size=*/max_non_chunked_layer_size,
          /*n_chunks_between_layers=*/n_chunks_between_layers,
          /*log_n_max_in_memory_fri_layer_elements=*/log_n_max_in_memory_fri_layer_elements,
      },
  };
}

// ------------------------------------------------------------------------------------------
//  Prover
// ------------------------------------------------------------------------------------------

void StarkProver::PerformLowDegreeTest(const CompositionOracleProver& oracle) {
  AnnotationScope scope(channel_.get(), "FRI");

  // Check that the fri_step_list and last_layer_degree_bound parameters are consistent with the
  // oracle degree bound.
  const uint64_t expected_fri_degree_bound = GetFriExpectedDegreeBound(*params_->fri_params);
  const uint64_t oracle_degree_bound = oracle.ConstraintsDegreeBound() * params_->TraceLength();
  ASSERT_RELEASE(
      expected_fri_degree_bound == oracle_degree_bound,
      "Fri parameters do not match oracle degree. Expected FRI degree from "
      "FriParameters: " +
          std::to_string(expected_fri_degree_bound) +
          ". STARK: " + std::to_string(oracle_degree_bound));

  ProfilingBlock profiling_block("FRI virtual oracle computation");
  // Evaluate composition polynomial.
  auto composition_polynomial_evaluation =
      oracle.EvalComposition(config_->constraint_polynomial_task_size);
  profiling_block.CloseBlock();

  ProfilingBlock fri_profiling_block("FRI");
  // Prepare FRI.
  FriProver::FirstLayerCallback first_layer_queries_callback =
      [this, &oracle](const std::vector<uint64_t>& fri_queries) {
        ProfilingBlock profiling_block("FRI virtual oracle callback");
        AnnotationScope scope(channel_.get(), "Virtual Oracle");
        const auto queries =
            FriQueriesToEvaluationDomainQueries(fri_queries, params_->TraceLength());
        oracle.DecommitQueries(queries);
      };

  FriProver fri_prover(
      UseOwned(channel_), UseOwned(table_prover_factory_), UseOwned(params_->fri_params),
      std::move(composition_polynomial_evaluation), UseOwned(&first_layer_queries_callback),
      UseOwned(&config_->fri_prover_config));
  fri_prover.ProveFri();
}

CommittedTraceProver StarkProver::CommitOnTrace(
    Trace&& trace, const FftBases& bases, bool bit_reverse, const std::string& profiling_text) {
  ProfilingBlock commit_block(profiling_text);
  AnnotationScope scope(channel_.get(), "Commit on Trace");
  CommittedTraceProver committed_trace(
      config_->cached_lde_config, UseOwned(&params_->evaluation_domain), trace.Width(),
      *table_prover_factory_);
  committed_trace.Commit(std::move(trace), bases, bit_reverse);
  return committed_trace;
}

CompositionOracleProver StarkProver::OutOfDomainSamplingProve(
    CompositionOracleProver original_oracle) {
  AnnotationScope scope(channel_.get(), "Out Of Domain Sampling");
  const Field& field = params_->evaluation_domain.GetField();

  const size_t n_breaks = original_oracle.ConstraintsDegreeBound();

  ProfilingBlock composition_block("Composition polynomial computation");
  auto composition_eval = original_oracle.EvalComposition(config_->constraint_polynomial_task_size);
  composition_block.CloseBlock();

  ProfilingBlock breaker_block("Polynomial breaker");
  // Break to evaluations of n_broken_columns polynomials on a single coset.
  // NOLINTNEXTLINE.
  auto [broken_uncommitted_trace, broken_bases] = oods::BreakCompositionPolynomial(
      composition_eval, n_breaks, *params_->composition_eval_bases);
  breaker_block.CloseBlock();

  // The resulting evaluations are on a domain which may have a different offset than the trace.
  // broken_bases represents that domain. It should have the same basis, but a different offset
  // than the trace.
  ASSERT_RELEASE(
      params_->evaluation_domain.Bases().At(0).BasisSize() == broken_bases->At(0).BasisSize(),
      "Trace and Broken bases do no match");

  // Lde and Commit on Broken.
  auto broken_trace = CommitOnTrace(
      std::move(broken_uncommitted_trace), *broken_bases, false, "Commit on composition");
  auto boundary_conditions =
      oods::ProveOods(channel_.get(), original_oracle, broken_trace, params_->use_extension_field);
  auto boundary_air = oods::CreateBoundaryAir(
      field, params_->evaluation_domain.Group().Size(), original_oracle.Width() + n_breaks,
      std::move(boundary_conditions));

  // Steal the traces (move) from the original_oracle oracle.
  auto traces = std::move(original_oracle).MoveTraces();
  traces.emplace_back(UseMovedValue(std::move(broken_trace)));
  for (const MaybeOwnedPtr<CommittedTraceProverBase>& trace : traces) {
    trace->FinalizeEval();
  }

  auto ods_composition_polynomial = CreateCompositionPolynomial(
      channel_.get(), field, params_->evaluation_domain.TraceGenerator(), *boundary_air);

  const auto boundary_mask = boundary_air->GetMask();
  CompositionOracleProver ods_virtual_oracle(
      UseOwned(&params_->evaluation_domain), std::move(traces), boundary_mask,
      TakeOwnershipFrom(std::move(boundary_air)),
      TakeOwnershipFrom(std::move(ods_composition_polynomial)), channel_.get());

  return ods_virtual_oracle;
}

void StarkProver::ValidateFirstTraceSize(const size_t n_rows, const size_t n_columns) {
  ASSERT_RELEASE(
      params_->evaluation_domain.Group().Size() == n_rows,
      "Trace length parameter " + std::to_string(n_rows) +
          " is inconsistent with actual trace length " +
          std::to_string(params_->evaluation_domain.Group().Size()) + ".");
  ASSERT_RELEASE(
      params_->air->GetNColumnsFirst() == n_columns,
      "Trace width parameter inconsistent with actual trace width.");
}

void StarkProver::ProveStark(std::unique_ptr<TraceContext> trace_context) {
  // First trace.
  ProfilingBlock profiling_block("Trace generation");
  Trace trace = trace_context->GetTrace();
  profiling_block.CloseBlock();

  const size_t trace_length = trace.Length();
  const size_t first_trace_width = trace.Width();
  ValidateFirstTraceSize(trace_length, first_trace_width);

  AnnotationScope scope(channel_.get(), "STARK");

  std::vector<MaybeOwnedPtr<CommittedTraceProverBase>> traces;
  // Add first committed trace.
  {
    AnnotationScope scope(channel_.get(), "Original");
    CommittedTraceProver committed_trace(CommitOnTrace(
        std::move(trace), params_->evaluation_domain.Bases(), true, "Commit on trace"));
    traces.emplace_back(UseMovedValue(std::move(committed_trace)));
  }

  //  Prepare for interaction.
  const Air* current_air = (params_->air).get();
  auto interaction_params = params_->air->GetInteractionParams();

  // Interaction phase.
  if (interaction_params.has_value()) {
    ASSERT_RELEASE(
        !params_->use_extension_field, "Extension field is not implemented for interaction.");

    AnnotationScope scope(channel_.get(), "Interaction");

    // Initialize interaction elements in trace context.
    trace_context->SetInteractionElements(starkware::GetInteractionElements(
        interaction_params->n_interaction_elements, params_->field, channel_.get()));
    auto interaction_trace = trace_context->GetInteractionTrace();
    ASSERT_RELEASE(
        interaction_params->n_columns_second == interaction_trace.Width(),
        "Number of columns in interaction trace is wrong.");
    const size_t trace_width = first_trace_width + interaction_trace.Width();
    VLOG(0) << "Trace cells count:\nLog number of rows: " + std::to_string(SafeLog2(trace_length)) +
                   "\nNumber of first trace columns: " + std::to_string(first_trace_width) +
                   "\nNumber of interaction columns: " + std::to_string(interaction_trace.Width()) +
                   "\nTotal trace cells: " + std::to_string(trace_length * trace_width)
            << std::endl;

    // Add interaction committed trace.
    CommittedTraceProver committed_interaction_trace(CommitOnTrace(
        std::move(interaction_trace), params_->evaluation_domain.Bases(), true,
        "Commit on interaction trace"));
    traces.emplace_back(UseMovedValue(std::move(committed_interaction_trace)));

    current_air = &(trace_context->GetAir());
  }

  // Create composition polynomial from AIR.
  std::unique_ptr<CompositionPolynomial> composition_polynomial;
  {
    AnnotationScope scope(channel_.get(), "Original");
    composition_polynomial = CreateCompositionPolynomial(
        channel_.get(), params_->field, params_->evaluation_domain.TraceGenerator(), *current_air);
  }

  CompositionOracleProver composition_oracle(
      UseOwned(&params_->evaluation_domain), std::move(traces), current_air->GetMask(),
      UseOwned(current_air), UseOwned(composition_polynomial.get()), channel_.get());

  const CompositionOracleProver oods_composition_oracle =
      OutOfDomainSamplingProve(std::move(composition_oracle));

  PerformLowDegreeTest(oods_composition_oracle);
}

// ------------------------------------------------------------------------------------------
//  Verifier
// ------------------------------------------------------------------------------------------

CommittedTraceVerifier StarkVerifier::ReadTraceCommitment(
    const size_t n_columns, bool should_verify_base_field) {
  ASSERT_RELEASE(
      !should_verify_base_field || IsExtensionField(params_->field),
      "The parameter should_verify_base_field is true but the field is not in the form of "
      "ExtensionFieldElement<>.");
  CommittedTraceVerifier trace_verifier(
      UseOwned(&params_->evaluation_domain), n_columns, *table_verifier_factory_,
      should_verify_base_field);
  AnnotationScope scope(channel_.get(), "Commit on Trace");
  trace_verifier.ReadCommitment();
  return trace_verifier;
}

void StarkVerifier::PerformLowDegreeTest(const CompositionOracleVerifier& oracle) {
  AnnotationScope scope(channel_.get(), "FRI");

  // Check that the fri_step_list and last_layer_degree_bound parameters are consistent with the
  // oracle degree bound.
  const uint64_t expected_fri_degree_bound = GetFriExpectedDegreeBound(*params_->fri_params);
  const uint64_t oracle_degree_bound = oracle.ConstraintsDegreeBound() * params_->TraceLength();
  ASSERT_RELEASE(
      expected_fri_degree_bound == oracle_degree_bound,
      "Fri parameters do not match oracle degree. Expected FRI degree from "
      "FriParameters: " +
          std::to_string(expected_fri_degree_bound) +
          ". STARK: " + std::to_string(oracle_degree_bound));

  // Prepare FRI.
  FriVerifier::FirstLayerCallback first_layer_queries_callback =
      [this, &oracle](const std::vector<uint64_t>& fri_queries) {
        AnnotationScope scope(channel_.get(), "Virtual Oracle");
        const auto queries =
            FriQueriesToEvaluationDomainQueries(fri_queries, params_->TraceLength());
        return oracle.VerifyDecommitment(queries);
      };
  FriVerifier fri_verifier(
      UseOwned(channel_), UseOwned(table_verifier_factory_), UseOwned(params_->fri_params),
      UseOwned(&first_layer_queries_callback));
  fri_verifier.VerifyFri();
}

CompositionOracleVerifier StarkVerifier::OutOfDomainSamplingVerify(
    CompositionOracleVerifier original_oracle) {
  AnnotationScope scope(channel_.get(), "Out Of Domain Sampling");
  std::optional<CommittedTraceVerifier> trace_verifier;
  {
    trace_verifier.emplace(
        UseOwned(&params_->evaluation_domain), original_oracle.ConstraintsDegreeBound(),
        *table_verifier_factory_);
    {
      AnnotationScope scope(channel_.get(), "Commit on Trace");
      trace_verifier->ReadCommitment();
    }
  }
  auto boundary_conditions = oods::VerifyOods(
      params_->evaluation_domain, channel_.get(), original_oracle, *params_->composition_eval_bases,
      params_->use_extension_field);

  auto boundary_air = oods::CreateBoundaryAir(
      params_->evaluation_domain.GetField(), params_->evaluation_domain.Group().Size(),
      original_oracle.Width() + original_oracle.ConstraintsDegreeBound(),
      std::move(boundary_conditions));
  {
    auto ods_composition_polynomial = CreateCompositionPolynomial(
        channel_.get(), params_->evaluation_domain.GetField(),
        params_->evaluation_domain.TraceGenerator(), *boundary_air);

    auto traces = std::move(original_oracle).MoveTraces();
    traces.emplace_back(UseMovedValue(std::move(*trace_verifier)));
    const auto boundary_mask = boundary_air->GetMask();
    CompositionOracleVerifier composition_oracle(
        UseOwned(&params_->evaluation_domain), std::move(traces), boundary_mask,
        TakeOwnershipFrom(std::move(boundary_air)),
        TakeOwnershipFrom(std::move(ods_composition_polynomial)), channel_.get());

    return composition_oracle;
  }
}

void StarkVerifier::VerifyStark() {
  AnnotationScope scope(channel_.get(), "STARK");
  std::vector<MaybeOwnedPtr<const CommittedTraceVerifierBase>> traces;
  // Create a FieldCommitmentSchemeVerifier for the decommitment.
  {
    AnnotationScope scope(channel_.get(), "Original");
    CommittedTraceVerifier first_trace_verifier(ReadTraceCommitment(
        params_->air->GetNColumnsFirst(),
        skip_assert_for_extension_field_test_ ? false : params_->use_extension_field));
    traces.emplace_back(UseMovedValue(std::move(first_trace_verifier)));
  }

  // Prepare for interaction.
  MaybeOwnedPtr<const Air> current_air(UseOwned(params_->air));
  auto interaction_params = params_->air->GetInteractionParams();

  // Interaction phase.
  if (interaction_params.has_value()) {
    ASSERT_RELEASE(
        !params_->use_extension_field, "Extension field is not implemented for interaction.");
    AnnotationScope scope(channel_.get(), "Interaction");

    // Update air according to interaction.
    current_air = TakeOwnershipFrom(params_->air->WithInteractionElements(GetInteractionElements(
        interaction_params->n_interaction_elements, params_->field, channel_.get())));

    CommittedTraceVerifier interaction_trace_verifier(
        ReadTraceCommitment(interaction_params->n_columns_second));
    traces.emplace_back(UseMovedValue(std::move(interaction_trace_verifier)));
  }

  // Composition polynomial.
  {
    AnnotationScope scope(channel_.get(), "Original");
    composition_polynomial_ = CreateCompositionPolynomial(
        channel_.get(), params_->field, params_->evaluation_domain.TraceGenerator(), *current_air);
  }
  CompositionOracleVerifier composition_oracle(
      UseOwned(&params_->evaluation_domain), std::move(traces), current_air->GetMask(),
      UseOwned(current_air.get()), TakeOwnershipFrom(std::move(composition_polynomial_)),
      channel_.get());

  const CompositionOracleVerifier oods_composition_oracle =
      OutOfDomainSamplingVerify(std::move(composition_oracle));

  PerformLowDegreeTest(oods_composition_oracle);
}

}  // namespace starkware
