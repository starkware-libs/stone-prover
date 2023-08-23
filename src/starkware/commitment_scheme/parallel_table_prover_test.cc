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

#include "starkware/commitment_scheme/parallel_table_prover.h"

#include <cstddef>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/commitment_scheme/table_prover_impl.h"
#include "starkware/commitment_scheme/table_prover_mock.h"

namespace starkware {
namespace {

using testing::StrictMock;

using FieldElementT = TestFieldElement;

std::vector<FieldElementVector> RandomData(Prng* prng, uint64_t n_rows, size_t n_columns) {
  std::vector<FieldElementVector> columns;
  for (size_t col = 0; col < n_columns; col++) {
    std::vector<FieldElementT> column;
    for (size_t row = 0; row < n_rows; row++) {
      column.push_back(FieldElementT::RandomElement(prng));
    }
    columns.push_back(FieldElementVector::Make<FieldElementT>(std::move(column)));
  }
  return columns;
}

class ParallelTableProverTest : public testing::Test {
 protected:
  ParallelTableProverTest()
      : parallel_table_prover_(
            UseOwned(&inner_table_prover_), n_tasks_per_segment_, sub_segment_size_),
        columns_(RandomData(&prng_, n_segments_ * n_rows_per_segment_, n_columns_)) {}

  const size_t n_tasks_per_segment_ = 2;
  const uint64_t n_segments_ = 8;
  const uint64_t n_rows_per_segment_ = 16;
  const uint64_t n_columns_ = 3;
  const uint64_t sub_segment_size_ = n_rows_per_segment_ / n_tasks_per_segment_;
  Prng prng_;
  StrictMock<TableProverMock> inner_table_prover_;
  ParallelTableProver parallel_table_prover_;

  std::vector<FieldElementVector> columns_;
};

#ifndef __EMSCRIPTEN__
/*
  Tests ParallelTableProver by mocking the inner table prover.
*/
TEST_F(ParallelTableProverTest, AddSegmentForCommitment) {
  const size_t original_segment_idx = 3;

  std::vector<ConstFieldElementSpan> segment_span;
  segment_span.reserve(n_columns_);
  for (const auto& column : columns_) {
    segment_span.push_back(column.AsSpan());
  }

  for (size_t i = 0; i < n_tasks_per_segment_; i++) {
    const size_t sub_segment_idx = original_segment_idx * n_tasks_per_segment_ + i;

    std::vector<ConstFieldElementSpan> sub_segments;
    sub_segments.reserve(n_columns_);
    for (auto&& column : segment_span) {
      sub_segments.push_back(column.SubSpan(sub_segment_size_ * i, sub_segment_size_));
    }
    EXPECT_CALL(inner_table_prover_, AddSegmentForCommitment(sub_segments, sub_segment_idx, 1));
  }

  parallel_table_prover_.AddSegmentForCommitment(segment_span, original_segment_idx, 1);
}
#endif

}  // namespace
}  // namespace starkware
