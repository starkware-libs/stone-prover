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

#ifndef STARKWARE_COMMITMENT_SCHEME_ROW_COL_H_
#define STARKWARE_COMMITMENT_SCHEME_ROW_COL_H_

#include <string>
#include <utility>

#include "starkware/math/math.h"

namespace starkware {

/*
  Represents a cell in a 2-dimensional array (row and column), with lexicographic comparison.
*/
struct RowCol {
 public:
  RowCol(uint64_t row, uint64_t col) : row_(row), col_(col) {}

  bool operator<(const RowCol& other) const { return AsPair() < other.AsPair(); }
  bool operator==(const RowCol& other) const { return AsPair() == other.AsPair(); }

  std::string ToString() const {
    return "(" + std::to_string(row_) + ", " + std::to_string(col_) + ")";
  }

  uint64_t GetRow() const { return row_; }
  uint64_t GetCol() const { return col_; }

 private:
  uint64_t row_;
  uint64_t col_;

  std::pair<uint64_t, uint64_t> AsPair() const { return {row_, col_}; }
};

inline std::ostream& operator<<(std::ostream& out, const RowCol& row_col) {
  return out << row_col.ToString();
}

}  // namespace starkware

#endif  // STARKWARE_COMMITMENT_SCHEME_ROW_COL_H_
