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

#ifndef STARKWARE_AIR_COMPONENTS_VIRTUAL_COLUMN_H_
#define STARKWARE_AIR_COMPONENTS_VIRTUAL_COLUMN_H_

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/math/math.h"

namespace starkware {

/*
  Represents an infinite arithmetic progression on row indices. For example [5, 7, 9, 11, ...].
*/
struct RowView {
  RowView(uint64_t step, uint64_t offset) : step(step), offset(offset) {}

  /*
    Gets the size of the view given the size of the origin.
    For example, if the step is 2 then the size of the view is half the original size.
  */
  uint64_t Size(uint64_t original_length) const { return SafeDiv(original_length, step); }

  /*
    Given a subview, returns the relative view with respect to this view.
    For example, for the view [5, 7, 9, 11 ..] and the subview [7, 11, ...],
    the relative view would be [1, 3, 5, ...], since the elements of the subview are
    [view[1], view[3], view[5], ...].
  */
  RowView Relative(const RowView& subview) const {
    return RowView(SafeDiv(subview.step, step), Inverse(subview.offset));
  }

  /*
    Finds the index of element in the progression.
  */
  uint64_t Inverse(const uint64_t element) const { return SafeDiv(element - offset, step); }

  /*
    Gets the element at index. For example, [1, 3, 5, ...].At(2) := 5.
  */
  uint64_t At(const uint64_t index) const { return offset + index * step; }

  /*
    The distance between two consecutive elements of the view.
  */
  uint64_t step;

  /*
    The first element.
  */
  uint64_t offset;
};

/*
  Represents a virtual column in the trace. A virtual column represents a subset of the rows in one
  (real) column. The subset has the form:
    { row_offset + step * i : i = 0, 1, ... }.

  The class is used for trace generation.
*/
struct VirtualColumn {
  VirtualColumn(uint64_t column, uint64_t step, uint64_t row_offset)
      : column(column), view(step, row_offset) {}

  /*
    Sets the value of one cell in the virtual column.
    The physical place of row is at offset + row * step.
  */
  template <typename FieldElementT>
  void SetCell(
      gsl::span<const gsl::span<FieldElementT>> trace, uint64_t row,
      const FieldElementT& value) const {
    ASSERT_DEBUG(column < trace.size(), "Column index exceeds number of columns in trace.");
    const uint64_t trace_row = view.At(row);
    ASSERT_DEBUG(trace_row < trace.at(column).size(), "Index of out bounds.");
    trace.at(column).at(trace_row) = value;
  }

  /*
    Gets the size of the virtual column.
  */
  uint64_t Size(uint64_t trace_length) const { return view.Size(trace_length); }

  /*
    Converts the logical index inside the virtual column to the physical index inside the trace.
    The result is given by: offset + row * step.
  */
  uint64_t ToTraceRowIndex(uint64_t row) const { return view.At(row); }

  /*
    The index of the column in its trace.
    If there is no interaction it is equal to the index of the column in the AIR.
    For example, if the first trace has 3 columns and the interaction trace has 2 columns, column =
    1 for the second column in the interaction trace.
  */
  uint64_t column;

  RowView view;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_COMPONENTS_VIRTUAL_COLUMN_H_
