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

#ifndef STARKWARE_AIR_BOUNDARY_CONSTRAINTS_BOUNDARY_PERIODIC_COLUMN_H_
#define STARKWARE_AIR_BOUNDARY_CONSTRAINTS_BOUNDARY_PERIODIC_COLUMN_H_

#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/composition_polynomial/periodic_column.h"

namespace starkware {

/*
  Creates a periodic column, with a single period (in other words, the periodicity is not used),
  which satisfies:
    periodic_column(rows[i]) = values[i] * c[i]
  where c[i] is invertible (!= 0) and depends on rows but not on values.

  In particular, one may take one periodic column with (rows, values) and another with
  (rows, ones) (where ones is a vector of 1's. See CreateBaseBoundaryPeriodicColumn()) and
  obtain:
    periodic_column(rows[i]) / ones_periodic_column(rows[i]) = values[i].

  The two periodic columns may be used to enforce boundary constraints on the trace. Take
  (rows, values) to be the boundary constraints, and add the following constraint to the AIR:
    mask_item * ones_periodic_column - periodic_column
  on the domain given by rows (prod_i(x - g^rows[i])).
*/
template <typename FieldElementT>
PeriodicColumn<FieldElementT> CreateBoundaryPeriodicColumn(
    gsl::span<const uint64_t> rows, gsl::span<const FieldElementT> values, uint64_t trace_length,
    const FieldElementT& trace_generator, const FieldElementT& trace_offset);

/*
  Same as CreateBoundaryPeriodicColumn() but where the y values are all 1.
*/
template <typename FieldElementT>
PeriodicColumn<FieldElementT> CreateBaseBoundaryPeriodicColumn(
    gsl::span<const uint64_t> rows, uint64_t trace_length, const FieldElementT& trace_generator,
    const FieldElementT& trace_offset);

/*
  Creates a periodic column, with a single period (in other words, the periodicity is not used),
  which satisfies:
    periodic_column(rows[i]) = 0, and is invertible elsewhere.
*/
template <typename FieldElementT>
PeriodicColumn<FieldElementT> CreateVanishingPeriodicColumn(
    gsl::span<const uint64_t> rows, uint64_t trace_length, const FieldElementT& trace_generator,
    const FieldElementT& trace_offset);

/*
  Creates a periodic column, with a single period (in other words, the periodicity is not used),
  which is zero on the rows {0, step, 2*step, ...} except for the given rows, where it is
  invertible.
*/
template <typename FieldElementT>
PeriodicColumn<FieldElementT> CreateComplementVanishingPeriodicColumn(
    gsl::span<const uint64_t> rows, uint64_t step, uint64_t trace_length,
    const FieldElementT& trace_generator, const FieldElementT& trace_offset);

}  // namespace starkware

#include "starkware/air/boundary_constraints/boundary_periodic_column.inl"

#endif  // STARKWARE_AIR_BOUNDARY_CONSTRAINTS_BOUNDARY_PERIODIC_COLUMN_H_
