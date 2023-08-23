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

#include "starkware/commitment_scheme/table_prover_impl.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/utils/invoke_template_version.h"
#include "starkware/channel/annotation_scope.h"
#include "starkware/commitment_scheme/table_impl_details.h"
#include "starkware/crypt_tools/utils.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {

using starkware::table::details::AllQueryRows;
using starkware::table::details::ElementDecommitAnnotation;
using starkware::table::details::ElementsToBeTransmitted;

namespace {

template <typename T>
size_t GetNumRows(const std::vector<gsl::span<const T>>& columns) {
  ASSERT_RELEASE(!columns.empty(), "columns must contain at least one column.");
  return columns[0].size();
}

template <typename T>
bool VerifyAllColumnsSameLength(const std::vector<gsl::span<const T>>& columns) {
  const size_t n_rows = GetNumRows(columns);
  for (const auto& column : columns) {
    if (column.size() != n_rows) {
      return false;
    }
  }

  return true;
}

/*
  Returns serialization of field elements in table, represented by a vector of columns. It stores
  all elements from same row in a consecutive block of bytes, and keeps the order of rows, and the
  order of columns inside each row. Formally, if each field element takes 'b' bytes, and there are
  'c' columns, the element from column 'x' and row 'y' is stored at 'b' bytes, starting of index
  '(y*c+x)*b'.
*/
template <typename FieldElementT>
std::vector<std::byte> SerializeFieldColumnsImpl(
    const std::vector<gsl::span<const FieldElementT>>& columns) {
  ASSERT_RELEASE(VerifyAllColumnsSameLength(columns), "The sizes of the columns must be the same.");
  const size_t n_columns = columns.size();
  const size_t n_rows = GetNumRows(columns);
  const size_t element_size_in_bytes = FieldElementT::SizeInBytes();
  const size_t n_bytes_row = columns.size() * element_size_in_bytes;

  std::vector<std::byte> serialization(n_rows * n_bytes_row);
  auto serialization_span = gsl::make_span(serialization);

  for (size_t row = 0; row < n_rows; row++) {
    for (size_t col = 0; col < n_columns; col++) {
      const size_t element_idx = row * n_columns + col;
      const size_t element_byte_idx = element_idx * element_size_in_bytes;
      columns[col][row].ToBytes(
          serialization_span.subspan(element_byte_idx, element_size_in_bytes));
    }
  }

  return serialization;
}

/*
  This is the polymorphic version of SerializeFieldColumnsImpl.
*/
std::vector<std::byte> SerializeFieldColumns(gsl::span<const ConstFieldElementSpan> segment) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) {
        using FieldElementT = typename decltype(field_tag)::type;
        std::vector<gsl::span<const FieldElementT>> columns;
        columns.reserve(segment.size());
        for (const ConstFieldElementSpan& segment_column : segment) {
          columns.push_back(segment_column.As<FieldElementT>());
        }
        return SerializeFieldColumnsImpl<FieldElementT>(columns);
      },
      segment[0].GetField());
}

}  // namespace

void TableProverImpl::AddSegmentForCommitment(
    const std::vector<ConstFieldElementSpan>& segment, size_t segment_index,
    size_t n_interleaved_columns) {
  ASSERT_RELEASE(
      segment.size() * n_interleaved_columns == n_columns_,
      "segment length is expected to be equal to the number of columns.");
  commitment_scheme_->AddSegmentForCommitment(SerializeFieldColumns(segment), segment_index);
}

void TableProverImpl::Commit() { commitment_scheme_->Commit(); }

std::vector<uint64_t> TableProverImpl::StartDecommitmentPhase(
    const std::set<RowCol>& data_queries, const std::set<RowCol>& integrity_queries) {
  // Make sure data_queries and integrity_queries are disjoint.
  ASSERT_RELEASE(
      AreDisjoint(data_queries, integrity_queries),
      "data_queries and integrity_queries must be disjoint");

  data_queries_ = data_queries;
  integrity_queries_ = integrity_queries;

  // Compute the block numbers in which the union of queries and integrity_queries participates.
  all_query_rows_ = AllQueryRows(data_queries_, integrity_queries_);

  const std::vector<uint64_t> requested_elements =
      commitment_scheme_->StartDecommitmentPhase(all_query_rows_);

  // Start by requesting all rows in which there are queries.
  std::vector<uint64_t> rows_to_request(all_query_rows_.begin(), all_query_rows_.end());

  // Add rows requested by the inner commitment scheme.
  std::copy(
      requested_elements.begin(), requested_elements.end(), std::back_inserter(rows_to_request));

  ASSERT_RELEASE(
      !HasDuplicates(gsl::make_span(requested_elements)),
      "Found duplicate row indices in requested_elements.");

  return rows_to_request;
}

void TableProverImpl::Decommit(gsl::span<const ConstFieldElementSpan> elements_data) {
  // elements_data is a 2D array (indexed by column and then row). The first rows refer to
  // all_query_rows_ and the last rows refer to rows requested by the inner commitment scheme.

  // Collect data for the inner commitment scheme.
  std::vector<ConstFieldElementSpan> elements_data_last_rows;
  ASSERT_RELEASE(
      elements_data.size() == n_columns_,
      "Expected the size of elements_data to be the number of columns.");
  for (const auto& column : elements_data) {
    ASSERT_RELEASE(
        column.Size() >= all_query_rows_.size(),
        "The number of rows does not match the number of requested rows in "
        "StartDecommitmentPhase().");
    elements_data_last_rows.push_back(column.SubSpan(all_query_rows_.size()));
  }

  // Transmit data for the queries, sorted by row and then column.
  // Note: currently we cannot simply iterate over to_transmit, since a row, for which all cells are
  // integrity queries, will not appear at all in this set.
  std::set<RowCol> to_transmit =
      ElementsToBeTransmitted(n_columns_, all_query_rows_, integrity_queries_);
  auto row_it = all_query_rows_.begin();
  auto to_transmit_it = to_transmit.begin();
  for (size_t i = 0; row_it != all_query_rows_.end(); i++, row_it++) {
    for (size_t col = 0; col < n_columns_; col++) {
      const RowCol query_loc(*row_it, col);
      // Don't transmit data for integrity queries.
      if (integrity_queries_.count(query_loc) != 0) {
        continue;
      }
      ASSERT_RELEASE(
          *to_transmit_it == query_loc, "Expected to transmit " + to_transmit_it->ToString() +
                                            " but found " + query_loc.ToString());
      channel_->SendFieldElement(elements_data[col][i], ElementDecommitAnnotation(query_loc));
      to_transmit_it++;
    }
  }

  commitment_scheme_->Decommit(SerializeFieldColumns(elements_data_last_rows));
}

}  // namespace starkware
