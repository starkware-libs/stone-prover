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

#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_MOCK_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_MOCK_H_

#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

class TableProverMock : public TableProver {
 public:
  MOCK_METHOD2(
      AddSegmentForCommitment,
      void(const std::vector<ConstFieldElementSpan>& segment, size_t segment_index));
  MOCK_METHOD3(
      AddSegmentForCommitment, void(
                                   const std::vector<ConstFieldElementSpan>& segment,
                                   size_t segment_index, size_t n_interleaved_columns));
  MOCK_METHOD0(Commit, void());
  MOCK_METHOD2(
      StartDecommitmentPhase,
      std::vector<uint64_t>(
          const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries));
  MOCK_METHOD1(Decommit, void(gsl::span<const ConstFieldElementSpan> elements_data));
};

/*
  A factory that holds several mocks and each time it is called returns the next mock.
  This class can be used to test functions that get a TableProverFactory.

  Usage: To test that TestedFunction(const TableProverMockFactory&) calls the
  factory twice with arguments 32 and 16, and calls Commit on each of the results:

    TableProverMockFactory table_prover_factory({32, 16});

    // Set expectations for the inner mocks.
    EXPECT_CALL(table_prover_factory[0], Commit());
    EXPECT_CALL(table_prover_factory[1], Commit());

    // Call the tested function.
    TestedFunction(table_prover_factory.AsFactory());
*/
// Disable linter since the destructor is effectively empty and therefore copy constructor and
// operator=() are not necessary.
class TableProverMockFactory {  // NOLINT
 public:
  explicit TableProverMockFactory(
      std::vector<std::tuple<uint64_t, uint64_t, size_t>> expected_params)
      : expected_params_(std::move(expected_params)) {
    for (uint64_t i = 0; i < expected_params_.size(); i++) {
      mocks_.push_back(std::make_unique<testing::StrictMock<TableProverMock>>());
    }
  }

  ~TableProverMockFactory() { EXPECT_EQ(mocks_.size(), cur_index_); }

  TableProverFactory AsFactory() {
    return [this](size_t n_segments, uint64_t n_rows_per_segment, size_t n_columns) {
      ASSERT_RELEASE(
          cur_index_ < mocks_.size(),
          "Operator() of TableProverMockFactory was called too many times.");
      EXPECT_EQ(
          expected_params_[cur_index_], std::make_tuple(n_segments, n_rows_per_segment, n_columns));
      return std::move(mocks_[cur_index_++]);
    };
  }

  /*
    Returns the mock at the given index.
    Don't use this function after AsFactory() was called.
  */
  TableProverMock& operator[](size_t index) {
    ASSERT_RELEASE(
        cur_index_ == 0,
        "TableProverMockFactory: Operator[] cannot be used after "
        "AsFactory()");
    return *mocks_[index];
  }

 private:
  std::vector<std::unique_ptr<testing::StrictMock<TableProverMock>>> mocks_;
  std::vector<std::tuple<uint64_t, uint64_t, size_t>> expected_params_;
  size_t cur_index_ = 0;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_PROVER_MOCK_H_
