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

#include "starkware/commitment_scheme/table_verifier_impl.h"

#include <cstddef>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/channel/prover_channel_mock.h"
#include "starkware/channel/verifier_channel_mock.h"
#include "starkware/commitment_scheme/commitment_scheme_builder.h"
#include "starkware/commitment_scheme/commitment_scheme_mock.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/crypt_tools/blake2s.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {
namespace {

using testing::ElementsAreArray;
using testing::HasSubstr;
using testing::InSequence;
using testing::Return;
using testing::StrictMock;

using FieldElementT = TestFieldElement;

class TableVerifierImplTest : public ::testing::Test {
 public:
  const Prng channel_prng{};

  std::vector<std::byte> GetValidProof(
      const Field& field, size_t n_columns, size_t n_segments, size_t n_rows_per_segment,
      const std::vector<std::vector<FieldElementT>>& table_data,
      const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) const;

  bool VerifyProof(
      const Field& field, size_t n_columns, size_t n_rows,
      const std::vector<std::vector<FieldElementT>>& table_data,
      const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries,
      const std::vector<std::byte>& proof) const;
};

#ifndef __EMSCRIPTEN__
/*
  The test uses mocks for the channel and the commitment scheme and goes through this flow:
  1. Read commitment (check that the underlying commitment scheme is called)
  2. Send artificial data queries and integrity queries.
  3. Check that only the data queries and the "clue" (i.e. - not query and not data, but in the
  same row with one of them) queries are sent to the commitment scheme.
  4. Check that when sending all queries (including integrity) and their expected values, indeed
  the underlying commitment scheme is called with the right index to all-bytes-of-that-row map.
  5. Check that when the underlying scheme returns true - so does the TableVerifier.
*/
TEST_F(TableVerifierImplTest, BasicFlow) {
  const size_t n_columns = 3;
  const uint64_t n_rows = 6;
  const unsigned r = 1000;
  StrictMock<VerifierChannelMock> verifier_channel;
  StrictMock<CommitmentSchemeVerifierMock> commitment_scheme;
  Field field = Field::Create<FieldElementT>();
  TableVerifierImpl table_verifier(
      field, n_columns, UseOwned(&commitment_scheme), &verifier_channel);
  EXPECT_CALL(commitment_scheme, ReadCommitment());
  table_verifier.ReadCommitment();
  std::set<RowCol> data_queries = {{0, 0}, {1, 0}, {1, 2}, {2, 1}};
  std::set<RowCol> integrity_queries = {{0, 2}, {1, 1}, {4, 0}, {4, 1}, {4, 2}, {5, 0}};
  std::set<size_t> skipped_rows = {3};
  // We use a 5 X 3 table, where cell i,j inhabits a field element whose index is r * i + j (r is
  // set to 1000 to make debugging easier).
  std::map<RowCol, FieldElement> channel_response;
  for (size_t i = 0; i < n_rows; ++i) {
    if (0 != Count(skipped_rows, i)) {
      continue;
    }
    for (size_t j = 0; j < n_columns; ++j) {
      if (0 == Count(integrity_queries, RowCol(i, j))) {
        // The TableVerifier expects all the non-integrity-queries elements, who are in a row with
        // some queried element, to be in the response.
        channel_response.insert({{i, j}, FieldElement(FieldElementT::FromUint(i * r + j))});
      }
    }
  }
  // Set the expectations for the calls to the verifier channel, and the elements to send through
  // its mock.
  {
    InSequence dummy;
    for (auto& it : channel_response) {
      EXPECT_CALL(verifier_channel, ReceiveFieldElementImpl(testing::_))
          .WillOnce(Return(it.second));
    }
  }
  std::map<RowCol, FieldElement> response = table_verifier.Query(data_queries, integrity_queries);
  // Test that the response is what we'd expect, i.e. - everything but the integrity queries is
  // there.
  EXPECT_THAT(response, ElementsAreArray(channel_response));

  // Add the integrity queries to the elements we wish to verify.
  std::map<RowCol, FieldElement> to_verify(response);
  for (const auto& iq : integrity_queries) {
    to_verify.insert({iq, FieldElement(FieldElementT::FromUint(iq.GetRow() * r + iq.GetCol()))});
  }

  // Add the underlying map that TableVerifier is expected to send to the commitment scheme, where
  // each row's number is the key, and the entire row serialized to bytes is the value.
  std::map<uint64_t, std::vector<std::byte>> integrity_map;
  for (size_t i = 0; i < n_rows; ++i) {
    if (0 != Count(skipped_rows, i)) {
      continue;
    }
    std::vector<std::byte> v(n_columns * FieldElementT::SizeInBytes());
    auto v_byte_span = gsl::make_span(v);
    for (size_t j = 0; j < n_columns; ++j) {
      to_verify.at(RowCol(i, j))
          .ToBytes(
              v_byte_span.subspan(j * FieldElementT::SizeInBytes(), FieldElementT::SizeInBytes()));
    }
    integrity_map.insert({i, v});
  }
  EXPECT_CALL(commitment_scheme, VerifyIntegrity(integrity_map)).WillOnce(Return(true));
  bool result = table_verifier.VerifyDecommitment(to_verify);
  EXPECT_TRUE(result);
}
#endif

/*
  Generate random data and integrity queries for a given table dimensions.
*/
void GetRandomQueries(
    size_t n_rows, size_t n_columns, size_t n_data_queries, size_t n_integrity_queries, Prng* prng,
    std::set<RowCol>* data_queries_out, std::set<RowCol>* integrity_queries_out) {
  ASSERT_RELEASE(
      n_data_queries + n_integrity_queries < n_columns * n_rows,
      "Table is too small to contain this many distinct queries.");
  ASSERT_RELEASE(n_columns * n_rows != 0, "Zero size table is used.");
  ASSERT_RELEASE(
      data_queries_out != nullptr && integrity_queries_out != nullptr,
      "Function must get pointers to allocated sets.");
  // Generate random data queries.
  while (data_queries_out->size() < n_data_queries) {
    RowCol row_col(
        prng->UniformInt<size_t>(0, n_rows - 1), prng->UniformInt<size_t>(0, n_columns - 1));
    auto iter_bool = data_queries_out->insert(row_col);
    VLOG_IF(2, iter_bool.second) << "Adding data query for location " << row_col;
  }
  while (integrity_queries_out->size() < n_integrity_queries) {
    RowCol row_col(
        prng->UniformInt<size_t>(0, n_rows - 1), prng->UniformInt<size_t>(0, n_columns - 1));
    // Make sure data and integrity queries are distinct.
    if (Count(*data_queries_out, row_col) == 0) {
      auto iter_bool = integrity_queries_out->insert(row_col);
      VLOG_IF(2, iter_bool.second) << "Adding integrity query for location " << row_col;
    }
  }
}

/*
  Create a valid proof using a non-interactive channel, with answers to the given queries.
*/
std::vector<std::byte> TableVerifierImplTest::GetValidProof(
    const Field& field, size_t n_columns, size_t n_segments, size_t n_rows_per_segment,
    const std::vector<std::vector<FieldElementT>>& table_data, const std::set<RowCol>& data_queries,
    const std::set<RowCol>& integrity_queries) const {
  const size_t size_of_row = field.ElementSizeInBytes() * n_columns;
  const uint64_t n_rows = n_segments * n_rows_per_segment;
  EXPECT_EQ(table_data.size(), n_columns)
      << "Num of columns mismatches the size of table_data, this "
         "is a bug in the test, not the program.";
  // Each segment is a row of column-vectors, which comprise a sub-table so to speak.
  std::vector<std::vector<ConstFieldElementSpan>> segment_data;
  segment_data.reserve(n_segments);
  for (size_t i = 0; i < n_segments; ++i) {
    segment_data.emplace_back();
    auto& segment = segment_data.back();
    segment.reserve(n_columns);
    for (auto& column : table_data) {
      gsl::span<const FieldElementT> s =
          gsl::make_span(column).subspan(i * n_rows_per_segment, n_rows_per_segment);
      segment.emplace_back(s);
    }
  }

  // Setup prover.
  NoninteractiveProverChannel prover_channel(channel_prng.Clone());

  Prng prng;
  const size_t n_out_of_memory_merkle_layers = 0;
  auto commitment_scheme_prover = MakeCommitmentSchemeProver<Blake2s256>(
      size_of_row, n_rows_per_segment, n_segments, &prover_channel,
      /*n_verifier_friendly_commitment_layers=*/0, CommitmentHashes(Blake2s256::HashName()),
      n_out_of_memory_merkle_layers);

  std::unique_ptr<TableProver> table_prover = std::make_unique<TableProverImpl>(
      n_columns, UseOwned(&commitment_scheme_prover), &prover_channel);

  // Start protocol - prover side.
  for (size_t i = 0; i < segment_data.size(); ++i) {
    table_prover->AddSegmentForCommitment(segment_data[i], i);
  }

  table_prover->Commit();

  const std::vector<uint64_t> elements_idxs_for_decommitment =
      table_prover->StartDecommitmentPhase(data_queries, integrity_queries);
  std::vector<FieldElementVector> elements_data;
  for (size_t column = 0; column < n_columns; column++) {
    FieldElementVector res = FieldElementVector::Make<FieldElementT>();
    for (const uint64_t row : elements_idxs_for_decommitment) {
      ASSERT_RELEASE(row < n_rows, "Invalid row.");
      const size_t segment = row / n_rows_per_segment;
      const size_t index = row % n_rows_per_segment;
      res.PushBack(segment_data[segment][column][index]);
    }
    elements_data.push_back(std::move(res));
  }

  table_prover->Decommit(
      std::vector<ConstFieldElementSpan>(elements_data.begin(), elements_data.end()));

  // Obtain proof from channel.
  std::vector<std::byte> proof = prover_channel.GetProof();
  VLOG(4) << prover_channel;
  return proof;
}

/*
  Verify a proof with given parameters, using a non-interactive verifier.
*/
bool TableVerifierImplTest::VerifyProof(
    const Field& field, size_t n_columns, size_t n_rows,
    const std::vector<std::vector<FieldElementT>>& table_data, const std::set<RowCol>& data_queries,
    const std::set<RowCol>& integrity_queries, const std::vector<std::byte>& proof) const {
  // Setup verifier.
  NoninteractiveVerifierChannel verifier_channel(channel_prng.Clone(), proof);
  const size_t size_of_row = field.ElementSizeInBytes() * n_columns;
  auto commitment_scheme_verifier = MakeCommitmentSchemeVerifier<Blake2s256>(
      size_of_row, n_rows, &verifier_channel, /*n_verifier_friendly_commitment_layers=*/0,
      CommitmentHashes(Blake2s256::HashName()));

  std::unique_ptr<TableVerifier> table_verifier = std::make_unique<TableVerifierImpl>(
      field, n_columns, UseOwned(&commitment_scheme_verifier), &verifier_channel);

  // Start protocol - verifier side.
  table_verifier->ReadCommitment();
  std::map<RowCol, FieldElement> data_for_verification =
      table_verifier->Query(data_queries, integrity_queries);
  // Add the data for queries the verifier already knows (i.e. - integrity queries).
  for (const RowCol& q : integrity_queries) {
    // We invert row and column, because we actually store it in table_data as a row of columns.
    auto iter_bool = data_for_verification.insert(
        {q, FieldElement(gsl::at(table_data, q.GetCol())[q.GetRow()])});
    EXPECT_TRUE(iter_bool.second)
        << "Trying to insert an integrity query to a map that already contains its data. This may "
           "happen if the TableVerifier replied with an integrity query (which it shouldn't).";
  }
  bool result = table_verifier->VerifyDecommitment(data_for_verification);
  VLOG(4) << verifier_channel;
  return result;
}

TEST_F(TableVerifierImplTest, AllQueriesAnswered) {
  // Setup phase.
  Field field = Field::Create<FieldElementT>();
  Prng prng;
  constexpr size_t kNColumns = 2, kNSegments = 32, kNRowsPerSegment = 8,
                   kNRows = kNRowsPerSegment * kNSegments;
  std::vector<std::vector<FieldElementT>> table_data(kNColumns);
  // Generate random columns of data.
  for (auto& column : table_data) {
    column = prng.RandomFieldElementVector<FieldElementT>(kNRows);
  }
  // Prepare 3 data and 5 integrity queries.
  std::set<RowCol> data_queries, integrity_queries;
  GetRandomQueries(kNRows, kNColumns, 3, 5, &prng, &data_queries, &integrity_queries);
  // Get proof.
  std::vector<std::byte> proof = GetValidProof(
      field, kNColumns, kNSegments, kNRowsPerSegment, table_data, data_queries, integrity_queries);

  // Setup verifier.
  NoninteractiveVerifierChannel verifier_channel(channel_prng.Clone(), proof);
  const size_t size_of_row = field.ElementSizeInBytes() * kNColumns;

  auto commitment_scheme_verifier = MakeCommitmentSchemeVerifier<Blake2s256>(
      size_of_row, kNRows, &verifier_channel, /*n_verifier_friendly_commitment_layers=*/0,
      CommitmentHashes(Blake2s256::HashName()));

  std::unique_ptr<TableVerifier> table_verifier = std::make_unique<TableVerifierImpl>(
      field, kNColumns, UseOwned(&commitment_scheme_verifier), &verifier_channel);

  // Start protocol - verifier side.
  table_verifier->ReadCommitment();
  std::map<RowCol, FieldElement> data_for_verification =
      table_verifier->Query(data_queries, integrity_queries);
  // Make sure that all data queries were answered_correctly.
  for (const RowCol& q : data_queries) {
    ASSERT_GT(data_for_verification.count(q), 0U) << "Data query not found in response";
    ASSERT_EQ(data_for_verification.at(q), FieldElement(table_data.at(q.GetCol())[q.GetRow()]))
        << "Incorrect response to data query.";
  }
}

TEST_F(TableVerifierImplTest, EndToEnd) {
  // Setup phase.
  Field field = Field::Create<FieldElementT>();
  Prng prng;
  constexpr size_t kNColumns = 6, kNSegments = 128, kNRowsPerSegment = 8,
                   kNRows = kNRowsPerSegment * kNSegments;
  std::vector<std::vector<FieldElementT>> table_data(kNColumns);
  // Generate random columns of data.
  for (auto& column : table_data) {
    column = prng.RandomFieldElementVector<FieldElementT>(kNRows);
  }
  // Prepare 3 data and 5 integrity queries.
  std::set<RowCol> data_queries, integrity_queries;
  GetRandomQueries(kNRows, kNColumns, 3, 5, &prng, &data_queries, &integrity_queries);

  // Get proof.
  std::vector<std::byte> proof = GetValidProof(
      field, kNColumns, kNSegments, kNRowsPerSegment, table_data, data_queries, integrity_queries);

  // Run Verifier.
  bool result =
      VerifyProof(field, kNColumns, kNRows, table_data, data_queries, integrity_queries, proof);
  EXPECT_TRUE(result);
}

TEST_F(TableVerifierImplTest, DisjointIntegrityAndDataQueries) {
  // Setup phase.
  Field field = Field::Create<FieldElementT>();
  Prng prng;
  constexpr size_t kNColumns = 6, kNSegments = 128, kNRowsPerSegment = 8,
                   kNRows = kNRowsPerSegment * kNSegments;
  std::vector<std::vector<FieldElementT>> table_data(kNColumns);
  // Generate random columns of data.
  for (auto& column : table_data) {
    column = prng.RandomFieldElementVector<FieldElementT>(kNRows);
  }
  // Prepare 3 data and 5 integrity queries.
  std::set<RowCol> data_queries, integrity_queries, integrity_queries_with_duplicate;
  GetRandomQueries(kNRows, kNColumns, 3, 5, &prng, &data_queries, &integrity_queries);
  // Fake a duplicate query, that appears both as integrity and as data query.
  integrity_queries_with_duplicate = integrity_queries;
  integrity_queries_with_duplicate.insert(*data_queries.begin());
  std::vector<std::byte> proof;
  // Try to get proof, expect the process to fail due to the duplicate query.
  EXPECT_ASSERT(
      GetValidProof(
          field, kNColumns, kNSegments, kNRowsPerSegment, table_data, data_queries,
          integrity_queries_with_duplicate),
      HasSubstr("data_queries and integrity_queries must be disjoint"));
  // Get proof, but for real now.
  proof = GetValidProof(
      field, kNColumns, kNSegments, kNRowsPerSegment, table_data, data_queries, integrity_queries);

  // Run Verifier.

  // Setup verifier.
  NoninteractiveVerifierChannel verifier_channel(channel_prng.Clone(), proof);
  const size_t size_of_row = field.ElementSizeInBytes() * kNColumns;

  auto commitment_scheme_verifier = MakeCommitmentSchemeVerifier<Blake2s256>(
      size_of_row, kNRows, &verifier_channel, /*n_verifier_friendly_commitment_layers=*/0,
      CommitmentHashes(Blake2s256::HashName()));

  std::unique_ptr<TableVerifier> table_verifier = std::make_unique<TableVerifierImpl>(
      field, kNColumns, UseOwned(&commitment_scheme_verifier), &verifier_channel);

  // Start protocol - verifier side.
  table_verifier->ReadCommitment();
  EXPECT_ASSERT(
      table_verifier->Query(data_queries, integrity_queries_with_duplicate),
      HasSubstr("data_queries and integrity_queries must be disjoint"));
}

}  // namespace
}  // namespace starkware
