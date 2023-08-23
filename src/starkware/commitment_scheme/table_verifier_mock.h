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

#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_MOCK_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_MOCK_H_

#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/commitment_scheme/table_verifier.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

class TableVerifierMock : public TableVerifier {
 public:
  MOCK_METHOD0(ReadCommitment, void());
  MOCK_METHOD2(
      Query, std::map<RowCol, FieldElement>(
                 const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries));
  MOCK_METHOD1(VerifyDecommitment, bool(const std::map<RowCol, FieldElement>& data));
};

/*
  See documentation for TableProverMockFactory. This is an equivalent class, for the
  verifier's use.
*/
// Disable linter since the destructor is effectively empty and therefore copy constructor and
// operator=() are not necessary.
class TableVerifierMockFactory {  // NOLINT
 public:
  explicit TableVerifierMockFactory(
      std::vector<std::tuple<Field, uint64_t, size_t>> expected_params)
      : expected_params_(std::move(expected_params)) {
    for (uint64_t i = 0; i < expected_params_.size(); i++) {
      mocks_.push_back(std::make_unique<testing::StrictMock<TableVerifierMock>>());
    }
  }

  ~TableVerifierMockFactory() { EXPECT_EQ(mocks_.size(), cur_index_); }

  TableVerifierFactory AsFactory() {
    return [this](const Field& field, uint64_t n_rows, size_t n_columns) {
      ASSERT_RELEASE(
          cur_index_ < mocks_.size(),
          "Operator() of TableVerifierMockFactory was called too many times.");
      EXPECT_EQ(expected_params_[cur_index_], std::make_tuple(field, n_rows, n_columns));
      return std::move(mocks_[cur_index_++]);
    };
  }

  /*
    Returns the mock at the given index.
    Don't use this function after AsFactory() was called.
  */
  TableVerifierMock& operator[](size_t index) {
    ASSERT_RELEASE(
        cur_index_ == 0,
        "TableVerifierMockFactory: Operator[] cannot be used after "
        "AsFactory()");
    return *mocks_[index];
  }

 private:
  std::vector<std::tuple<Field, uint64_t, size_t>> expected_params_;
  std::vector<std::unique_ptr<testing::StrictMock<TableVerifierMock>>> mocks_;
  size_t cur_index_ = 0;
};

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_VERIFIER_MOCK_H_
