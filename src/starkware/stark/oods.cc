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

#include "starkware/stark/oods.h"

#include "starkware/air/boundary/boundary_air.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/composition_polynomial/breaker.h"
#include "starkware/utils/profiling.h"

namespace starkware {
namespace oods {

std::pair<Trace, std::unique_ptr<FftBases>> BreakCompositionPolynomial(
    const ConstFieldElementSpan& composition_evaluation, size_t n_breaks, const FftBases& bases) {
  const size_t log_n_breaks = SafeLog2(n_breaks);
  const std::unique_ptr<const PolynomialBreak> poly_break =
      MakePolynomialBreak(bases, log_n_breaks);

  FieldElementVector output = FieldElementVector::MakeUninitialized(
      composition_evaluation.GetField(), composition_evaluation.Size());
  const std::vector<ConstFieldElementSpan> output_spans =
      poly_break->Break(composition_evaluation, output);

  return {Trace::CopyFrom(output_spans), bases.FromLayerAsUniquePtr(log_n_breaks)};
}

std::unique_ptr<Air> CreateBoundaryAir(
    const Field& field, uint64_t trace_length, size_t n_columns,
    std::vector<std::tuple<size_t, FieldElement, FieldElement>>&& boundary_constraints) {
  auto boundary_air = InvokeFieldTemplateVersion(
      [&](auto field_tag) -> std::unique_ptr<Air> {
        using FieldElementT = typename decltype(field_tag)::type;
        return std::make_unique<BoundaryAir<FieldElementT>>(
            trace_length, n_columns, std::move(boundary_constraints));
      },
      field);

  return boundary_air;
}

/*
  Sends values to channel. If verifier_friendly_channel_updates is true, we hash them using Poseidon
  and send only the result to the channel, otherwise sends them one by one.
*/
void SendElementsToChannel(
    ProverChannel* channel, const ConstFieldElementSpan& values,
    const bool verifier_friendly_channel_updates) {
  if (verifier_friendly_channel_updates) {
    // Send all the elements as a single span.
    channel->SendFieldElementSpan(values);
  } else {
    // Sending each element separately to the channel.
    for (size_t i = 0; i < values.Size(); ++i) {
      channel->SendFieldElement(values[i], std::to_string(i));
    }
  }
}

std::vector<std::tuple<size_t, FieldElement, FieldElement>> ProveOods(
    ProverChannel* channel, const CompositionOracleProver& original_oracle,
    const CommittedTraceProverBase& broken_trace, const bool use_extension_field,
    const bool verifier_friendly_channel_updates) {
  AnnotationScope scope(channel, "OODS values");
  const Field& field = original_oracle.EvaluationDomain().GetField();
  const FieldElement& trace_gen = original_oracle.EvaluationDomain().TraceGenerator();
  if (use_extension_field) {
    ASSERT_RELEASE(GetFrobenius(trace_gen) == trace_gen, "trace_gen not in base field.");
  }
  std::vector<std::tuple<size_t, FieldElement, FieldElement>> boundary_constraints;
  const FieldElement point = channel->GetRandomFieldElementFromVerifier(field, "Evaluation point");
  const std::optional<FieldElement> conj_point =
      use_extension_field ? std::make_optional(GetFrobenius(point)) : std::nullopt;

  ProfilingBlock profiling_block("Eval at OODS point");

  const auto& mask = original_oracle.GetMask();
  const size_t mask_size = mask.size();
  const size_t n_breaks = broken_trace.NumColumns();
  FieldElementVector elements_to_send =
      FieldElementVector::MakeUninitialized(field, mask_size + n_breaks);

  // OODS trace side.
  {
    // Compute mask at point.
    FieldElementSpan trace_evaluation_at_mask = elements_to_send.AsSpan().SubSpan(0, mask_size);
    original_oracle.EvalMaskAtPoint(point, trace_evaluation_at_mask);
    std::vector<bool> cols_seen(original_oracle.Width(), false);
    // Creates the LHS of the boundary constraints to be returned.
    for (size_t i = 0; i < trace_evaluation_at_mask.Size(); ++i) {
      const auto& trace_eval_at_idx = trace_evaluation_at_mask[i];
      const auto& [row_offset, column_index] = mask[i];
      const auto& row_element = trace_gen.Pow(row_offset);
      boundary_constraints.emplace_back(column_index, point * row_element, trace_eval_at_idx);

      // In case use_extension_field is true, add a corresponding boundary constarint on the
      // conjugate element to guarantee that the trace is defined over the base field.
      // This is done only once for each column.
      if (use_extension_field && !cols_seen[column_index]) {
        cols_seen[column_index] = true;
        const auto conj_trace_eval_at_idx_value = GetFrobenius(trace_eval_at_idx);
        boundary_constraints.emplace_back(
            column_index, (*conj_point) * row_element, conj_trace_eval_at_idx_value);
      }
    }
  }

  // OODS broken side.
  {
    // Compute a simple mask consisting of one row for broken side.
    std::vector<std::pair<int64_t, uint64_t>> broken_eval_mask;
    broken_eval_mask.reserve(n_breaks);

    for (size_t column_index = 0; column_index < n_breaks; ++column_index) {
      broken_eval_mask.emplace_back(0, column_index);
    }

    // Compute at point.
    const FieldElement point_transformed = point.Pow(n_breaks);
    FieldElementSpan broken_evaluation = elements_to_send.AsSpan().SubSpan(mask_size);
    broken_trace.EvalMaskAtPoint(broken_eval_mask, point_transformed, broken_evaluation);

    // Creates the RHS of the boundary constraints.
    for (size_t i = 0; i < broken_evaluation.Size(); ++i) {
      // Assuming all broken_column appear right after trace columns.
      boundary_constraints.emplace_back(
          original_oracle.Width() + i, point_transformed, broken_evaluation[i]);
    }
  }
  SendElementsToChannel(channel, elements_to_send, verifier_friendly_channel_updates);

  return boundary_constraints;
}

/*
  Receives n_values from the channel. Flag verifier_friendly_channel_updates defined if we update
  the channel using Pedersen hash chain on the elements or not. For more info, see
  SendElementsToChannel function.
*/
FieldElementVector ReceiveElementsFromChannel(
    VerifierChannel* channel, const size_t n_values, Field field,
    const bool verifier_friendly_channel_updates) {
  FieldElementVector values_from_prover = FieldElementVector::MakeUninitialized(field, n_values);
  if (verifier_friendly_channel_updates) {
    channel->ReceiveFieldElementSpan(field, values_from_prover.AsSpan());
  } else {
    // Receiving each element separately.
    for (size_t i = 0; i < n_values; ++i) {
      const FieldElement value = channel->ReceiveFieldElement(field, std::to_string(i));
      values_from_prover.Set(i, value);
    }
  }
  return values_from_prover;
}

std::vector<std::tuple<size_t, FieldElement, FieldElement>> VerifyOods(
    const ListOfCosets& evaluation_domain, VerifierChannel* channel,
    const CompositionOracleVerifier& original_oracle, const FftBases& composition_eval_bases,
    const bool use_extension_field, const bool verifier_friendly_channel_updates) {
  AnnotationScope scope(channel, "OODS values");
  const Field& field = evaluation_domain.GetField();
  const FieldElement& trace_gen = evaluation_domain.TraceGenerator();
  if (use_extension_field) {
    ASSERT_RELEASE(GetFrobenius(trace_gen) == trace_gen, "trace_gen not in base field.");
  }
  std::vector<std::tuple<size_t, FieldElement, FieldElement>> boundary_constraints;
  const FieldElement point = channel->GetRandomFieldElementFromVerifier(field, "Evaluation point");
  const std::optional<FieldElement> conj_point =
      use_extension_field ? std::make_optional(GetFrobenius(point)) : std::nullopt;

  const auto& mask = original_oracle.GetMask();
  const size_t trace_mask_size = mask.size();
  const size_t n_breaks = original_oracle.ConstraintsDegreeBound();

  // Receive values.
  const size_t n_values_from_prover = trace_mask_size + n_breaks;
  FieldElementVector values_from_prover = ReceiveElementsFromChannel(
      channel, n_values_from_prover, field, verifier_friendly_channel_updates);

  // OODS trace side.
  // This loop creates the LHS of the boundary constraints to be returned.
  std::vector<bool> cols_seen(original_oracle.Width(), false);
  // Get first trace_mask_size elements from prover to compute boundary constraints of the trace
  // polynomials.
  auto original_oracle_mask_evaluation =
      ConstFieldElementSpan(values_from_prover).SubSpan(0, trace_mask_size);
  for (size_t i = 0; i < trace_mask_size; ++i) {
    const auto& [row_offset, column_index] = mask[i];
    const FieldElement value = original_oracle_mask_evaluation[i];
    boundary_constraints.emplace_back(column_index, point * trace_gen.Pow(row_offset), value);

    if (use_extension_field && !cols_seen[column_index]) {
      cols_seen[column_index] = true;
      const auto conj_trace_eval_at_idx_value = GetFrobenius(value);

      boundary_constraints.emplace_back(
          column_index, (*conj_point) * trace_gen.Pow(row_offset), conj_trace_eval_at_idx_value);
    }
  }

  const FieldElement trace_side_value = original_oracle.GetCompositionPolynomial().EvalAtPoint(
      point, original_oracle_mask_evaluation);

  // PolynomialBreaker.
  auto poly_break = MakePolynomialBreak(composition_eval_bases, SafeLog2(n_breaks));

  // OODS broken side.
  // This loop creates the RHS of the boundary constraints to be returned.
  const FieldElement point_transformed = point.Pow(n_breaks);

  // Get last n_breaks elements from prover to compute boundary constraints of broken trace
  // polynomials.
  auto broken_evaluation =
      ConstFieldElementSpan(values_from_prover).SubSpan(trace_mask_size, n_breaks);
  for (size_t i = 0; i < n_breaks; ++i) {
    boundary_constraints.emplace_back(
        original_oracle.Width() + i, point_transformed, broken_evaluation[i]);
  }

  const FieldElement broken_side_value = poly_break->EvalFromSamples(broken_evaluation, point);
  ASSERT_RELEASE(
      trace_side_value == broken_side_value, "Out of domain sampling verification failed");
  return boundary_constraints;
}

}  // namespace oods
}  // namespace starkware
