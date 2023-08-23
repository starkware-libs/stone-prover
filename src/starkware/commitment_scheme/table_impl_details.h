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

#ifndef STARKWARE_COMMITMENT_SCHEME_TABLE_IMPL_DETAILS_H_
#define STARKWARE_COMMITMENT_SCHEME_TABLE_IMPL_DETAILS_H_

#include <set>
#include <string>
#include <vector>

#include "starkware/channel/prover_channel.h"
#include "starkware/commitment_scheme/commitment_scheme.h"
#include "starkware/commitment_scheme/table_prover.h"
#include "starkware/utils/maybe_owned_ptr.h"

namespace starkware {
namespace table {
namespace details {

/*
  Given the Row/Col locations of data queries and integrity queries, this function returns a set of
  all indices of rows that contain at least one query from these given location sets.
*/
std::set<uint64_t> AllQueryRows(
    const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries);

/*
  Returns a list of RowCol pointing to the field elements that have to be transmitted to allow the
  verification of the queries. These are all the RowCol locations that are in a row with some
  integrity/data query, but are not integrity query locations themselves.
*/
std::set<RowCol> ElementsToBeTransmitted(
    size_t n_columns, const std::set<uint64_t>& all_query_rows,
    const std::set<RowCol>& integrity_queries);

std::string ElementDecommitAnnotation(const RowCol& row_col);

}  // namespace details
}  // namespace table
}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_TABLE_IMPL_DETAILS_H_
