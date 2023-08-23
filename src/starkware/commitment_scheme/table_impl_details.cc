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

#include "starkware/commitment_scheme/table_impl_details.h"

#include <algorithm>
#include <limits>
#include <set>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/channel/annotation_scope.h"
#include "starkware/crypt_tools/utils.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {
namespace table {
namespace details {

std::set<uint64_t> AllQueryRows(
    const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) {
  std::set<uint64_t> all_query_rows = {};
  for (const RowCol& query : data_queries) {
    all_query_rows.insert(query.GetRow());
  }
  for (const RowCol& query : integrity_queries) {
    all_query_rows.insert(query.GetRow());
  }
  return all_query_rows;
}

std::set<RowCol> ElementsToBeTransmitted(
    size_t n_columns, const std::set<uint64_t>& all_query_rows,
    const std::set<RowCol>& integrity_queries) {
  std::set<RowCol> to_be_transmitted;
  for (size_t row : all_query_rows) {
    for (size_t col = 0; col < n_columns; col++) {
      const RowCol query_loc(row, col);
      // Add the location (row, col) only if it is not part of integrity_queries.
      if (integrity_queries.count(query_loc) == 0) {
        to_be_transmitted.insert(query_loc);
      }
    }
  }
  return to_be_transmitted;
}

std::string ElementDecommitAnnotation(const RowCol& row_col) {
  return "Row " + std::to_string(row_col.GetRow()) + ", Column " + std::to_string(row_col.GetCol());
}

}  // namespace details
}  // namespace table
}  // namespace starkware
