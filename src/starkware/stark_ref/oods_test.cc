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

#include <algorithm>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/polynomials.h"
#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel_mock.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/composition_polynomial/composition_polynomial_mock.h"
#include "starkware/stark/committed_trace_mock.h"
#include "starkware/stark/composition_oracle.h"
#include "starkware/stark/test_utils.h"
#include "starkware/stark/utils.h"
#include "starkware/statement/fibonacci/fibonacci_statement.h"

namespace starkware {
namespace {

using testing::_;
using testing::Property;
using testing::Return;
using testing::StrictMock;

#ifndef __EMSCRIPTEN__

/*
  Used to mock the output of EvalMaskAtPoint().
  Example use: EXPECT_CALL(..., EvalMaskAtPoint(...)).WillOnce(SetEvaluation(mock_evaluation)).
*/
ACTION_P(SetEvaluation, eval) {
  const FieldElementSpan& outputs = arg2;
  ASSERT_RELEASE(outputs.Size() == eval.size(), "Wrong evaluation size");
  std::copy(eval.begin(), eval.end(), outputs.template As<TestFieldElement>().begin());
}

/*
  Tests that ProveOods sends exactly what we expect:
  * The evaluation of the mask of the original trace at z.
  * The evaluations of the broken columns at z.
*/
TEST(OutOfDomainSampling, ProveOods) {
  using FieldElementT = TestFieldElement;
  const size_t trace_length = 16;
  const size_t n_cosets = 4;
  const size_t mask_size = 7;
  const size_t n_columns = 11;
  const size_t n_breaks = 2;

  // Setup prng, field, domain, mask.
  Prng channel_prng;
  const Field field = Field::Create<FieldElementT>();
  const FieldElement expected_point =
      NoninteractiveProverChannel(channel_prng.Clone()).GetRandomFieldElementFromVerifier(field);
  Prng prng;
  const ListOfCosets evaluation_domain = ListOfCosets::MakeListOfCosets(
      trace_length, n_cosets, Field::Create<FieldElementT>(),
      MultiplicativeGroupOrdering::kBitReversedOrder);

  std::vector<std::pair<int64_t, uint64_t>> mask;
  mask.reserve(mask_size);
  for (size_t i = 0; i < mask_size; ++i) {
    mask.emplace_back(
        prng.UniformInt<int64_t>(0, n_cosets - 1), prng.UniformInt<int64_t>(0, n_columns - 1));
  }

  // Random values to send as OODS values.
  auto expected_mask_eval_values = prng.RandomFieldElementVector<FieldElementT>(mask_size);
  auto expected_broken_values = prng.RandomFieldElementVector<FieldElementT>(n_breaks);

  // Mocks.
  StrictMock<CommittedTraceProverMock> original_trace;
  StrictMock<CommittedTraceProverMock> broken_trace;
  std::vector<MaybeOwnedPtr<CommittedTraceProverBase>> original_oracle_traces;
  original_oracle_traces.emplace_back(UseOwned(&original_trace));
  EXPECT_CALL(original_trace, NumColumns()).WillRepeatedly(Return(n_columns));
  EXPECT_CALL(broken_trace, NumColumns()).WillRepeatedly(Return(n_breaks));

  // Oracle.
  NoninteractiveProverChannel prover_channel(NoninteractiveProverChannel(channel_prng.Clone()));
  CompositionOracleProver oracle(
      UseOwned(&evaluation_domain), std::move(original_oracle_traces), mask, nullptr, nullptr,
      &prover_channel);

  // Tests original_trace.EvalMaskAtPoint() call.
  EXPECT_CALL(
      original_trace,
      EvalMaskAtPoint(_, expected_point, Property(&FieldElementSpan::Size, mask.size())))
      .WillOnce(SetEvaluation(expected_mask_eval_values));

  // Tests broken_trace.EvalMaskAtPoint() call.
  EXPECT_CALL(
      broken_trace,
      EvalMaskAtPoint(_, expected_point.Pow(n_breaks), Property(&FieldElementSpan::Size, n_breaks)))
      .WillOnce(SetEvaluation(expected_broken_values));

  // Calls ProveOods().
  const auto boundary_constraints =
      oods::ProveOods(&prover_channel, oracle, broken_trace, /*use_extension_field=*/false);
  EXPECT_EQ(boundary_constraints.size(), mask_size + n_breaks);

  // Tests the sent_data seems right.
  const auto sent_data = prover_channel.GetProof();

  NoninteractiveProverChannel prover_channel_2(NoninteractiveProverChannel(channel_prng.Clone()));
  for (const auto& value : expected_mask_eval_values) {
    prover_channel_2.SendFieldElement(FieldElement(value));
  }
  for (const auto& value : expected_broken_values) {
    prover_channel_2.SendFieldElement(FieldElement(value));
  }

  const auto expected_sent_data = prover_channel_2.GetProof();

  EXPECT_EQ(sent_data, expected_sent_data);
}

/*
  Tests that VerifyOods verifies the right formula, and returns the right value.
*/
TEST(OutOfDomainSampling, VerifyOods) {
  using FieldElementT = TestFieldElement;
  const size_t trace_length = 16;
  const size_t n_cosets = 4;
  const size_t mask_size = 7;
  const size_t n_columns = 11;
  const size_t n_breaks = 4;

  // Setup prng, field, domain, mask, composition_eval_bases.
  Prng channel_prng;
  const Field field = Field::Create<FieldElementT>();
  NoninteractiveProverChannel fake_prover_channel(channel_prng.Clone());
  FieldElement expected_point = fake_prover_channel.GetRandomFieldElementFromVerifier(field);
  Prng prng;
  const ListOfCosets evaluation_domain = ListOfCosets::MakeListOfCosets(
      trace_length, n_cosets, Field::Create<FieldElementT>(),
      MultiplicativeGroupOrdering::kBitReversedOrder);
  const auto composition_eval_bases = MultiplicativeFftBases<FieldElementT>(
      SafeLog2(trace_length) + SafeLog2(n_breaks), FieldElementT::One());

  std::vector<std::pair<int64_t, uint64_t>> mask;
  mask.reserve(mask_size);
  for (size_t i = 0; i < mask_size; ++i) {
    mask.emplace_back(
        prng.UniformInt<int64_t>(0, n_cosets - 1), prng.UniformInt<int64_t>(0, n_columns - 1));
  }

  // Random values to receive as OODS values.
  const auto fake_mask_eval_values = prng.RandomFieldElementVector<FieldElementT>(mask_size);
  const auto fake_broken_values = prng.RandomFieldElementVector<FieldElementT>(n_breaks);
  FieldElementVector fake_mask_eval_vec = FieldElementVector::CopyFrom(fake_mask_eval_values);

  // Compute what the value on the RHS of the OODS equation will be.
  FieldElement expected_check_value =
      FieldElement(HornerEval(expected_point.As<FieldElementT>(), fake_broken_values));

  // Create a fake proof.
  for (const auto& value : fake_mask_eval_values) {
    fake_prover_channel.SendFieldElement(FieldElement(value));
  }
  for (const auto& value : fake_broken_values) {
    fake_prover_channel.SendFieldElement(FieldElement(value));
  }

  NoninteractiveVerifierChannel verifier_channel(
      channel_prng.Clone(), fake_prover_channel.GetProof());

  // Mocks.
  StrictMock<CommittedTraceVerifierMock> original_trace;
  std::vector<MaybeOwnedPtr<const CommittedTraceVerifierBase>> original_oracle_traces;
  original_oracle_traces.emplace_back(UseOwned(&original_trace));
  EXPECT_CALL(original_trace, NumColumns()).WillRepeatedly(Return(n_columns));

  StrictMock<CompositionPolynomialMock> composition_polynomial_mock;
  EXPECT_CALL(composition_polynomial_mock, GetDegreeBound())
      .WillRepeatedly(Return(trace_length * n_breaks));

  // Oracle.
  CompositionOracleVerifier oracle(
      UseOwned(&evaluation_domain), std::move(original_oracle_traces), mask, nullptr,
      UseOwned(&composition_polynomial_mock), &verifier_channel);

  // Mocks returned value on the LHS of the OODS equation, to be the same as the RHS we computed
  // before.
  EXPECT_CALL(
      composition_polynomial_mock,
      EvalAtPoint(expected_point, ConstFieldElementSpan(fake_mask_eval_vec)))
      .WillOnce(Return(expected_check_value));

  // Calls VerifyOods().
  const auto boundary_constraints = oods::VerifyOods(
      evaluation_domain, &verifier_channel, oracle, composition_eval_bases,
      /*use_extension_field=*/false);
  EXPECT_EQ(boundary_constraints.size(), mask_size + n_breaks);
}

#endif

template <typename FieldElementT>
void FibonacciOodsEndToEnd(bool use_extension_field, const FieldElementT& secret, Prng* prng) {
  const size_t trace_length = 16;
  const size_t fibonacci_claim_index = 12;
  const size_t n_cosets = 8;

  // Setup field, domain, mask, composition_eval_bases.
  Prng channel_prng;
  const Field field = Field::Create<FieldElementT>();
  NoninteractiveProverChannel prover_channel(channel_prng.Clone());
  const ListOfCosets evaluation_domain = ListOfCosets::MakeListOfCosets(
      trace_length, n_cosets, Field::Create<FieldElementT>(),
      MultiplicativeGroupOrdering::kBitReversedOrder);

  // Fibonacci air.
  const FieldElementT claimed_fib =
      FibonacciAir<FieldElementT>::PublicInputFromPrivateInput(secret, fibonacci_claim_index);
  const FibonacciAir<FieldElementT> fibonacci_air(trace_length, fibonacci_claim_index, claimed_fib);

  // Commit on trace.
  const size_t n_columns = fibonacci_air.NumColumns();
  const size_t n_layers = 0;
  const size_t n_verifier_friendly_commitment_layers = 0;
  const TableProverFactory table_prover_factory = GetTableProverFactory<Keccak256>(
      &prover_channel, FieldElementT::SizeInBytes(),
      /*table_prover_n_tasks_per_segment*/ 32, n_layers, n_verifier_friendly_commitment_layers,
      CommitmentHashes(Keccak256::HashName()));
  CommittedTraceProver trace_prover(
      {false, false}, UseOwned(&evaluation_domain), n_columns, table_prover_factory);
  trace_prover.Commit(
      FibonacciAir<FieldElementT>::GetTrace(secret, trace_length, fibonacci_claim_index),
      evaluation_domain.Bases(), true);

  // Composition polynomial.
  const size_t num_random_coefficients_required = fibonacci_air.NumRandomCoefficients();
  const FieldElementVector random_coefficients = FieldElementVector::Make(
      prng->RandomFieldElementVector<FieldElementT>(num_random_coefficients_required));

  const std::unique_ptr<const CompositionPolynomial> composition_polynomial =
      fibonacci_air.CreateCompositionPolynomial(
          evaluation_domain.TraceGenerator(), random_coefficients);

  // OracleProver.
  std::vector<MaybeOwnedPtr<CommittedTraceProverBase>> trace_provers;
  trace_provers.emplace_back(UseOwned(&trace_prover));
  const CompositionOracleProver oracle_prover(
      UseOwned(&evaluation_domain), std::move(trace_provers), fibonacci_air.GetMask(),
      UseOwned(&fibonacci_air), UseOwned(composition_polynomial.get()), &prover_channel);

  // Break polynomial.
  const size_t n_breaks = oracle_prover.ConstraintsDegreeBound();
  const auto composition_eval_bases =
      MultiplicativeFftBases<FieldElementT, MultiplicativeGroupOrdering::kBitReversedOrder>(
          SafeLog2(trace_length) + SafeLog2(n_breaks), FieldElementT::GetBaseGenerator());
  const auto composition_polynomial_eval = oracle_prover.EvalComposition(1);

  CommittedTraceProver broken_prover(
      {false, false}, UseOwned(&evaluation_domain), n_breaks, table_prover_factory);
  {
    auto&& [broken_uncommitted_trace, broken_bases] = oods::BreakCompositionPolynomial(
        composition_polynomial_eval, n_breaks, composition_eval_bases);
    broken_prover.Commit(std::move(broken_uncommitted_trace), *broken_bases, false);
  }

  // ProveOods.
  const auto prover_boundary_constraints =
      oods::ProveOods(&prover_channel, oracle_prover, broken_prover, use_extension_field);

  // Verifier.
  NoninteractiveVerifierChannel verifier_channel(channel_prng.Clone(), prover_channel.GetProof());

  // Trace.
  const TableVerifierFactory table_verifier_factory = [&verifier_channel](
                                                          const Field& field, uint64_t n_rows,
                                                          uint64_t n_columns) {
    return MakeTableVerifier<Keccak256, FieldElementT>(field, n_rows, n_columns, &verifier_channel);
  };
  CommittedTraceVerifier trace_verifier(
      UseOwned(&evaluation_domain), n_columns, table_verifier_factory, use_extension_field);
  trace_verifier.ReadCommitment();

  CommittedTraceVerifier broken_verifier(
      UseOwned(&evaluation_domain), n_breaks, table_verifier_factory);
  trace_verifier.ReadCommitment();

  // OracleVerifier.
  std::vector<MaybeOwnedPtr<const CommittedTraceVerifierBase>> trace_verifiers;
  trace_verifiers.emplace_back(UseOwned(&trace_verifier));
  const CompositionOracleVerifier oracle_verifier(
      UseOwned(&evaluation_domain), std::move(trace_verifiers), fibonacci_air.GetMask(),
      UseOwned(&fibonacci_air), UseOwned(composition_polynomial.get()), &verifier_channel);

  // VerifyOods.
  const auto verifier_boundary_constraints = oods::VerifyOods(
      evaluation_domain, &verifier_channel, oracle_verifier, composition_eval_bases,
      use_extension_field);

  // Test same constraints.
  EXPECT_EQ(prover_boundary_constraints, verifier_boundary_constraints);
}

TEST(OutOfDomainSampling, EndToEnd) {
  Prng prng;
  FibonacciOodsEndToEnd<TestFieldElement>(false, TestFieldElement::RandomElement(&prng), &prng);
  FibonacciOodsEndToEnd<ExtensionFieldElement<TestFieldElement>>(
      true, ExtensionFieldElement<TestFieldElement>::RandomBaseElement(&prng), &prng);
  FibonacciOodsEndToEnd<ExtensionFieldElement<TestFieldElement>>(
      false, ExtensionFieldElement<TestFieldElement>::RandomBaseElement(&prng), &prng);
}

}  // namespace
}  // namespace starkware
