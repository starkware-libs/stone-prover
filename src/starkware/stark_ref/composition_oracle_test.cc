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

#include "starkware/stark/composition_oracle.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel_mock.h"
#include "starkware/composition_polynomial/composition_polynomial_mock.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/stark/committed_trace_mock.h"

namespace starkware {
namespace {

using testing::_;
using testing::AllOf;
using testing::Each;
using testing::HasSubstr;
using testing::Invoke;
using testing::Property;
using testing::Return;
using testing::StrictMock;

using FieldElementT = TestFieldElement;

#ifndef __EMSCRIPTEN__

/*
  Helper class for CompositionOracleProver tests. Holds parameters for test, and common setup code.
*/
class CompositionOracleProverTester {
 public:
  CompositionOracleProverTester(
      size_t trace_length, size_t n_cosets, size_t n_columns, size_t n_traces,
      MultiplicativeGroupOrdering order)
      : trace_length(trace_length),
        n_cosets(n_cosets),
        n_columns(n_columns),
        n_traces(n_traces),
        evaluation_domain(ListOfCosets::MakeListOfCosets(
            trace_length, n_cosets, Field::Create<FieldElementT>(), order)) {
    // Create traces mocks.
    for (size_t trace_i = 0; trace_i < n_traces; ++trace_i) {
      traces.emplace_back(std::make_unique<StrictMock<CommittedTraceProverMock>>());

      EXPECT_CALL(*traces[trace_i], NumColumns()).WillRepeatedly(Return(n_columns));
    }

    // Get pointers to traces.
    traces_ptrs.reserve(n_traces);
    for (auto& trace : traces) {
      traces_ptrs.emplace_back(UseOwned(trace.get()));
    }
  }

  void TestEvalComposition(size_t degree_bound);
  void TestDecommitQueries();
  void TestInvalidMask();

  size_t trace_length;
  size_t n_cosets;
  size_t n_columns;
  size_t n_traces;

  ListOfCosets evaluation_domain;
  ProverChannelMock channel;

  std::vector<std::unique_ptr<StrictMock<CommittedTraceProverMock>>> traces;
  std::vector<MaybeOwnedPtr<CommittedTraceProverBase>> traces_ptrs;
};

/*
  Tests CompositionOracleProverTester::EvalComposition(). Feeds random traces to
  CompositionOracleProverTester, and checks that
  CompositionPolynomial::EvalOnCosetBitReversedOutput() is called with parameters of correct sizes.
*/
void CompositionOracleProverTester::TestEvalComposition(size_t degree_bound) {
  const size_t task_size = 32;
  FieldElementVector coset_offsets_bit_reversed(FieldElementVector::MakeUninitialized(
      evaluation_domain.GetField(), evaluation_domain.NumCosets()));
  std::vector<CachedLdeManager> trace_ldes;

  // Setup.
  Prng prng;
  CachedLdeManager::Config cached_lde_manager_config = {
      /*store_full_lde=*/false,
      /*use_fft_for_eval=*/false};

  const size_t log_cosets = SafeLog2(evaluation_domain.NumCosets());
  for (uint64_t i = 0; i < coset_offsets_bit_reversed.Size(); ++i) {
    coset_offsets_bit_reversed.Set(i, evaluation_domain.CosetsOffsets()[BitReverse(i, log_cosets)]);
  }

  // LDE random traces.
  trace_ldes.reserve(n_traces * n_columns);
  for (size_t trace_i = 0; trace_i < n_traces; ++trace_i) {
    std::unique_ptr<LdeManager> lde_manager = MakeLdeManager(evaluation_domain.Bases());
    CachedLdeManager cached_lde_manager(
        cached_lde_manager_config, TakeOwnershipFrom(std::move(lde_manager)),
        UseOwned(&coset_offsets_bit_reversed));
    for (size_t column_i = 0; column_i < n_columns; ++column_i) {
      FieldElementVector eval =
          FieldElementVector::Make(prng.RandomFieldElementVector<FieldElementT>(trace_length));
      cached_lde_manager.AddEvaluation(eval);
    }
    cached_lde_manager.FinalizeAdding();

    trace_ldes.emplace_back(std::move(cached_lde_manager));

    EXPECT_CALL(*traces[trace_i], GetLde()).WillRepeatedly(Return(&trace_ldes[trace_i]));
  }

  // Composition polynomial mock.
  StrictMock<CompositionPolynomialMock> composition_polynomial;
  EXPECT_CALL(composition_polynomial, GetDegreeBound())
      .WillRepeatedly(Return(degree_bound * trace_length));
  for (uint64_t coset_index = 0; coset_index < degree_bound; coset_index++) {
    const FieldElement coset_offset = coset_offsets_bit_reversed[coset_index];
    // Test that each coset is computed with correct offset, and correct sizes of arguments.
    EXPECT_CALL(
        composition_polynomial,
        EvalOnCosetBitReversedOutput(
            coset_offset,
            AllOf(
                Property(&gsl::span<const ConstFieldElementSpan>::size, n_traces * n_columns),
                Each(Property(&ConstFieldElementSpan::Size, trace_length))),
            Property(&FieldElementSpan::Size, trace_length), task_size));
  }

  // Create CompositionOracleProver.
  std::vector<std::pair<int64_t, uint64_t>> mask;

  CompositionOracleProver oracle_prover(
      UseOwned(&evaluation_domain), std::move(traces_ptrs), mask, nullptr,
      UseOwned(&composition_polynomial), &channel);

  FieldElementVector result = oracle_prover.EvalComposition(task_size);
  EXPECT_EQ(result.Size(), degree_bound * trace_length);
}

TEST(CompositionOracleProver, EvalComposition) {
  CompositionOracleProverTester(16, 4, 7, 2, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestEvalComposition(2);

  // Test a single coset.
  CompositionOracleProverTester(16, 1, 7, 2, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestEvalComposition(1);

  // Test a single column.
  CompositionOracleProverTester(16, 4, 1, 1, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestEvalComposition(2);

  // Test a single trace.
  CompositionOracleProverTester(16, 4, 7, 1, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestEvalComposition(2);

  // Test degree 1.
  CompositionOracleProverTester(16, 4, 7, 2, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestEvalComposition(1);

  // Test bit reversed.
  CompositionOracleProverTester(16, 4, 7, 2, MultiplicativeGroupOrdering::kBitReversedOrder)
      .TestEvalComposition(2);

  // Test no cosets.
  EXPECT_ASSERT(
      CompositionOracleProverTester(16, 0, 7, 2, MultiplicativeGroupOrdering::kNaturalOrder)
          .TestEvalComposition(2),
      HasSubstr("must be positive"));

  // Test no traces.
  CompositionOracleProverTester(16, 4, 7, 0, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestEvalComposition(2);

  // Test degree 0.
  CompositionOracleProverTester(16, 4, 7, 2, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestEvalComposition(0);
}

/*
  Test CompositionPolynomialMock::DecommmitQueries(). Check that DecommitQueries() is called with
  parameters of correct size.
*/
void CompositionOracleProverTester::TestDecommitQueries() {
  Prng prng;

  // Composition polynomial mock.
  StrictMock<CompositionPolynomialMock> composition_polynomial;

  // Create CompositionOracleProver.
  std::vector<std::pair<int64_t, uint64_t>> mask;
  for (size_t trace_i = 0; trace_i < n_traces; ++trace_i) {
    const size_t masks_items_in_trace = prng.UniformInt<size_t>(1, 5);
    const size_t column_offset = n_columns * trace_i;
    for (size_t i = 0; i < masks_items_in_trace; ++i) {
      mask.emplace_back(i, prng.UniformInt<size_t>(0, n_columns - 1) + column_offset);
    }

    EXPECT_CALL(
        *traces[trace_i],
        DecommitQueries(Property(
            &gsl::span<const std::tuple<uint64_t, uint64_t, size_t>>::size, masks_items_in_trace)));
  }

  CompositionOracleProver oracle_prover(
      UseOwned(&evaluation_domain), std::move(traces_ptrs), mask, nullptr,
      UseOwned(&composition_polynomial), &channel);

  // Check a single query.
  oracle_prover.DecommitQueries(
      {{prng.UniformInt<uint64_t>(0, n_cosets - 1),
        prng.UniformInt<uint64_t>(0, n_traces * n_columns - 1)}});
}

TEST(CompositionOracleProver, DecommitQueries) {
  CompositionOracleProverTester(16, 4, 7, 2, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestDecommitQueries();

  // Test a single coset.
  CompositionOracleProverTester(16, 1, 7, 2, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestDecommitQueries();

  // Test a single column.
  CompositionOracleProverTester(16, 4, 1, 1, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestDecommitQueries();

  // Test a single trace.
  CompositionOracleProverTester(16, 4, 7, 1, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestDecommitQueries();

  // Test bit reversed order.
  CompositionOracleProverTester(16, 4, 7, 2, MultiplicativeGroupOrdering::kBitReversedOrder)
      .TestDecommitQueries();

  // Test no cosets.
  EXPECT_ASSERT(
      CompositionOracleProverTester(16, 0, 7, 2, MultiplicativeGroupOrdering::kNaturalOrder)
          .TestDecommitQueries(),
      HasSubstr("must be positive"));

  // Test no traces.
  CompositionOracleProverTester(16, 4, 7, 0, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestDecommitQueries();
}

void CompositionOracleProverTester::TestInvalidMask() {
  Prng prng;

  // Composition polynomial mock.
  StrictMock<CompositionPolynomialMock> composition_polynomial;

  // Create CompositionOracleProver.
  std::vector<std::pair<int64_t, uint64_t>> mask;
  mask.reserve(n_traces * n_columns + 1);
  // Last column is out of range.
  for (size_t i = 0; i <= n_traces * n_columns; ++i) {
    mask.emplace_back(0, i);
  }

  EXPECT_ASSERT(
      CompositionOracleProver(
          UseOwned(&evaluation_domain), std::move(traces_ptrs), mask, nullptr,
          UseOwned(&composition_polynomial), &channel),
      HasSubstr("out of range"));
}

TEST(CompositionOracleProver, InvalidMask) {
  CompositionOracleProverTester(16, 4, 7, 2, MultiplicativeGroupOrdering::kNaturalOrder)
      .TestInvalidMask();
}

void VerifierTestVerifyDecommitment(
    size_t trace_length, size_t n_cosets, size_t n_columns, size_t n_traces,
    MultiplicativeGroupOrdering order, size_t n_queries) {
  ASSERT_RELEASE(n_queries >= 2, "Invalid number of queries for test");

  Prng prng;

  // Composition polynomial mock.
  StrictMock<CompositionPolynomialMock> composition_polynomial;

  // Create traces.
  const ListOfCosets evaluation_domain =
      ListOfCosets::MakeListOfCosets(trace_length, n_cosets, Field::Create<FieldElementT>(), order);

  // Create traces mocks.
  std::vector<std::unique_ptr<StrictMock<CommittedTraceVerifierMock>>> traces;
  for (size_t trace_i = 0; trace_i < n_traces; ++trace_i) {
    traces.emplace_back(std::make_unique<StrictMock<CommittedTraceVerifierMock>>());

    EXPECT_CALL(*traces[trace_i], NumColumns()).WillRepeatedly(Return(n_columns));
  }

  // Get pointers to traces.
  std::vector<MaybeOwnedPtr<const CommittedTraceVerifierBase>> traces_ptrs;
  traces_ptrs.reserve(n_traces);
  for (auto& trace : traces) {
    traces_ptrs.emplace_back(UseOwned(trace.get()));
  }

  // Create masks and trace decommitment callbacks.
  std::vector<std::pair<int64_t, uint64_t>> mask;
  for (size_t trace_i = 0; trace_i < n_traces; ++trace_i) {
    const size_t masks_items_in_trace = prng.UniformInt<size_t>(1, 5);
    const size_t column_offset = n_columns * trace_i;
    for (size_t i = 0; i < masks_items_in_trace; ++i) {
      mask.emplace_back(i, prng.UniformInt<size_t>(0, n_columns - 1) + column_offset);
    }

    EXPECT_CALL(
        *traces[trace_i], VerifyDecommitment(Property(
                              &gsl::span<const std::tuple<uint64_t, uint64_t, size_t>>::size,
                              n_queries * masks_items_in_trace)))
        .WillOnce(Invoke([](gsl::span<const std::tuple<uint64_t, uint64_t, size_t>> queries) {
          return FieldElementVector::MakeUninitialized(
              Field::Create<FieldElementT>(), queries.size());
        }));
  }

  // Generate random queries.
  std::vector<std::pair<uint64_t, uint64_t>> queries;
  queries.reserve(n_queries);
  for (size_t i = 0; i < n_queries - 1; ++i) {
    queries.emplace_back(
        prng.UniformInt<uint64_t>(0, n_cosets - 1), prng.UniformInt<uint64_t>(0, trace_length - 1));
  }
  // Force a duplicate.
  queries.push_back(queries.back());

  // CompositionPolynomial::EvalAtPoint() callback.
  FieldElement random_element = FieldElement(FieldElementT::RandomElement(&prng));
  EXPECT_CALL(
      composition_polynomial, EvalAtPoint(_, Property(&ConstFieldElementSpan::Size, mask.size())))
      .Times(n_queries)
      .WillRepeatedly(Return(random_element));

  // Create CompositionOracleVerifier.
  VerifierChannelMock channel;
  CompositionOracleVerifier oracle_verifier(
      UseOwned(&evaluation_domain), std::move(traces_ptrs), mask, nullptr,
      UseOwned(&composition_polynomial), &channel);
  oracle_verifier.VerifyDecommitment(queries);
}

TEST(CompositionOracleVerifier, VerifyDecommitment) {
  VerifierTestVerifyDecommitment(16, 4, 7, 2, MultiplicativeGroupOrdering::kNaturalOrder, 5);
}

#endif

}  // namespace
}  // namespace starkware
