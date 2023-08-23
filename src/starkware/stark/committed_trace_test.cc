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

#include "starkware/stark/committed_trace.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/commitment_scheme/table_verifier_impl.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/stark/test_utils.h"
#include "starkware/stark/utils.h"

namespace starkware {
namespace {

template <typename FieldElementT>
void TestEndToEnd(
    const CachedLdeManager::Config& config, MultiplicativeGroupOrdering order,
    bool verify_decommit_in_base_field = false) {
  SCOPED_TRACE(
      "config = {" + std::to_string(config.store_full_lde) + "," +
      std::to_string(config.use_fft_for_eval) + ", order = " +
      (order == MultiplicativeGroupOrdering::kNaturalOrder ? "Natural order"
                                                           : "Bit reversed order"));
  Prng prng;

  // Setup.
  Field field = Field::Create<FieldElementT>();
  const size_t trace_length = 16;
  const size_t n_cosets = 8;
  const size_t n_columns = 7;
  const size_t mask_size = 12;
  const size_t n_out_of_memory_merkle_layers = 0;
  // n_verifier_friendly_commitment_layers should either be 0 or be at least in the height of the
  // Merkle tree that is its leaves are the segments (cosets) roots.
  const size_t n_verifier_friendly_commitment_layers = 0;

  ListOfCosets evaluation_domain(
      ListOfCosets::MakeListOfCosets(trace_length, n_cosets, field, order));

  // Prover setup.
  Prng channel_prng;
  NoninteractiveProverChannel prover_channel(NoninteractiveProverChannel(channel_prng.Clone()));

  TableProverFactory table_prover_factory = GetTableProverFactory<Keccak256>(
      &prover_channel, FieldElementT::SizeInBytes(),
      /*table_prover_n_tasks_per_segment*/ 32, n_out_of_memory_merkle_layers,
      n_verifier_friendly_commitment_layers, CommitmentHashes(Keccak256::HashName()));

  // Random trace.
  std::vector<std::vector<FieldElementT>> trace_columns;
  for (size_t i = 0; i < n_columns; ++i) {
    trace_columns.push_back(prng.RandomFieldElementVector<FieldElementT>(trace_length));
  }

  // Commit.
  CommittedTraceProver ctrace_prover(
      config, UseOwned(&evaluation_domain), n_columns, table_prover_factory);
  ctrace_prover.Commit(Trace(std::move(trace_columns)), evaluation_domain.Bases(), false);

  auto lde = ctrace_prover.GetLde();

  // Test constants.
  EXPECT_EQ(ctrace_prover.NumColumns(), n_columns);
  EXPECT_EQ(ctrace_prover.GetLde()->NumColumns(), n_columns);

  // Decommit some, and test.
  const size_t n_queries = 12;
  ASSERT_RELEASE(n_queries >= 2, "Not enough queries for test");

  std::vector<std::tuple<uint64_t, uint64_t, size_t>> queries;
  queries.reserve(n_queries);
  for (size_t i = 0; i < n_queries - 1; ++i) {
    queries.emplace_back(
        prng.UniformInt<uint64_t>(0, n_cosets - 1), prng.UniformInt<uint64_t>(0, trace_length - 1),
        prng.UniformInt<uint64_t>(0, n_columns - 1));
  }
  queries.push_back(queries[prng.UniformInt<size_t>(0, n_queries - 2)]);

  ctrace_prover.DecommitQueries(queries);

  // EvalMaskAtPoint.
  std::vector<std::pair<int64_t, uint64_t>> mask;
  for (size_t i = 0; i < mask_size; ++i) {
    mask.emplace_back(
        prng.UniformInt<int64_t>(0, trace_length - 1), prng.UniformInt<int64_t>(0, n_columns - 1));
  }
  const FieldElementT point = FieldElementT::RandomElement(&prng);
  FieldElementT trace_gen = evaluation_domain.TraceGenerator().As<FieldElementT>();
  FieldElementVector eval_mask_output = FieldElementVector::MakeUninitialized(field, mask.size());
  ctrace_prover.EvalMaskAtPoint(mask, FieldElement(point), eval_mask_output);
  for (size_t i = 0; i < mask_size; ++i) {
    const auto& [mask_row, mask_col] = mask[i];
    const FieldElementVector shifted_point =
        FieldElementVector::Make(std::vector<FieldElementT>{point * Pow(trace_gen, mask_row)});
    FieldElementVector expected_output = FieldElementVector::MakeUninitialized(field, 1);
    lde->EvalAtPointsNotCached(mask_col, shifted_point, expected_output);
    ASSERT_EQ(expected_output[0], eval_mask_output[i]);
  }

  // Verify.
  NoninteractiveVerifierChannel verifier_channel(channel_prng.Clone(), prover_channel.GetProof());
  verifier_channel.SetExpectedAnnotations(prover_channel.GetAnnotations());
  TableVerifierFactory table_verifier_factory =
      [&verifier_channel](const Field& field, uint64_t n_rows, uint64_t n_columns) {
        return MakeTableVerifier<Keccak256, FieldElementT>(
            field, n_rows, n_columns, &verifier_channel, n_verifier_friendly_commitment_layers,
            CommitmentHashes(Keccak256::HashName()));
      };

  CommittedTraceVerifier ctrace_verifier(
      UseOwned(&evaluation_domain), n_columns, table_verifier_factory,
      /*should_verify_base_field=*/verify_decommit_in_base_field);

  ctrace_verifier.ReadCommitment();

  if (verify_decommit_in_base_field) {
    EXPECT_ASSERT(
        ctrace_verifier.VerifyDecommitment(queries),
        testing::HasSubstr("There is an element in the trace which is not in the base field."));
  } else {
    FieldElementVector results = ctrace_verifier.VerifyDecommitment(queries);
    for (size_t query_index = 0; query_index < queries.size(); ++query_index) {
      const auto& [coset_index, offset, column_index] = queries[query_index];
      // Coset order is bit reversed in CommittedTraceProver.
      const uint64_t coset_index_rev = BitReverse(coset_index, SafeLog2(n_cosets));
      // Compute expected value.
      FieldElement point = evaluation_domain.ElementByIndex(coset_index_rev, offset);
      FieldElementVector points = FieldElementVector::MakeUninitialized(field, 1);
      points.Set(0, point);
      FieldElementVector output = FieldElementVector::MakeUninitialized(field, 1);
      lde->EvalAtPointsNotCached(column_index, points, output);
      FieldElement expected_value = output[0];

      const FieldElement& value = results[query_index];
      EXPECT_EQ(value, expected_value);
    }
  }
}

TEST(CommittedTraceProver, Basic) {
  TestEndToEnd<TestFieldElement>({false, false}, MultiplicativeGroupOrdering::kNaturalOrder);
  TestEndToEnd<TestFieldElement>({false, true}, MultiplicativeGroupOrdering::kNaturalOrder);
  TestEndToEnd<TestFieldElement>({true, true}, MultiplicativeGroupOrdering::kNaturalOrder);
  TestEndToEnd<TestFieldElement>({false, false}, MultiplicativeGroupOrdering::kBitReversedOrder);
  TestEndToEnd<TestFieldElement>({false, true}, MultiplicativeGroupOrdering::kBitReversedOrder);
  TestEndToEnd<TestFieldElement>({true, true}, MultiplicativeGroupOrdering::kBitReversedOrder);

  // When verify_decommit_in_base_field is true the verifier expects the trace to be generated in
  // the base field (and not in the extension field, as done in the test), hence an exception should
  // be thrown. When verify_decommit_in_base_field is false, the decommit should pass.
  TestEndToEnd<ExtensionFieldElement<TestFieldElement>>(
      {true, true}, MultiplicativeGroupOrdering::kBitReversedOrder,
      /*verify_decommit_in_base_field=*/true);
  TestEndToEnd<ExtensionFieldElement<TestFieldElement>>(
      {true, true}, MultiplicativeGroupOrdering::kBitReversedOrder,
      /*verify_decommit_in_base_field=*/false);
}

}  // namespace
}  // namespace starkware
