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

#include <algorithm>
#include <cstddef>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/annotation_scope.h"
#include "starkware/commitment_scheme/table_impl_details.h"
#include "starkware/crypt_tools/utils.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {

using starkware::table::details::AllQueryRows;
using starkware::table::details::ElementDecommitAnnotation;
using starkware::table::details::ElementsToBeTransmitted;

void TableVerifierImpl::ReadCommitment() { commitment_scheme_->ReadCommitment(); }

std::map<RowCol, FieldElement> TableVerifierImpl::Query(
    const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) {
  ASSERT_VERIFIER(
      AreDisjoint(data_queries, integrity_queries),
      "data_queries and integrity_queries must be disjoint");
  std::map<RowCol, FieldElement> response;
  std::set<RowCol> to_receive = ElementsToBeTransmitted(
      n_columns_, AllQueryRows(data_queries, integrity_queries), integrity_queries);
  for (const RowCol& query_loc : to_receive) {
    auto iter_bool = response.insert(
        {query_loc, channel_->ReceiveFieldElement(field_, ElementDecommitAnnotation(query_loc))});
    ASSERT_RELEASE(iter_bool.second, "Received two messages with the same key");
  }
  return response;
}

bool TableVerifierImpl::VerifyDecommitment(const std::map<RowCol, FieldElement>& all_rows_data) {
  // We gather the elements of each row in sequence, as bytes, and put that in the map, with the row
  // number as key.
  std::map<uint64_t, std::vector<std::byte>> integrity_map{};
  // We rely on the fact that std::map is sorted by key, and our keys are compared row-first, to
  // assume that iterating over all_rows_data is iterating over cells and rows in the natural order
  // one reads numbers in a table: top to bottom, left to right.
  const size_t element_size = field_.ElementSizeInBytes();
  for (auto all_rows_it = all_rows_data.begin(); all_rows_it != all_rows_data.end();) {
    size_t cur_row = all_rows_it->first.GetRow();
    auto iter_bool =
        integrity_map.insert({cur_row, std::vector<std::byte>(n_columns_ * element_size)});
    ASSERT_VERIFIER(iter_bool.second, "Row already exists in the map.");
    for (size_t col = 0, pos = 0; col < n_columns_; ++col, ++all_rows_it, pos += element_size) {
      ASSERT_VERIFIER(all_rows_it != all_rows_data.end(), "Not enough columns in the map.");
      ASSERT_VERIFIER(
          all_rows_it->first.GetRow() == cur_row,
          "Data skips to next row before finishing the current.");
      all_rows_it->second.ToBytes(
          gsl::make_span(iter_bool.first->second).subspan(pos, element_size));
    }
  }

  return commitment_scheme_->VerifyIntegrity(integrity_map);
}

}  // namespace starkware
