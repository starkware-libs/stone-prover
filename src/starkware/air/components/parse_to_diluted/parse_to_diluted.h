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

#ifndef STARKWARE_AIR_COMPONENTS_PARSE_TO_DILUTED_PARSE_TO_DILUTED_H_
#define STARKWARE_AIR_COMPONENTS_PARSE_TO_DILUTED_PARSE_TO_DILUTED_H_

#include <functional>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/diluted_check/diluted_check_cell.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/field_operations.h"

namespace starkware {

template <typename FieldElementT>
class ParseToDilutedComponent {
 public:
  /*
    A component for parsing the input of the Keccak component.
    See src/starkware/air/components/parse_to_diluted/parse_to_diluted.py for documentation.
    dimension_sizes are the sizes of state_dims and dimension_capacities are its capacities.
  */
  ParseToDilutedComponent(
      const std::string& name, const TraceGenerationContext& ctx, gsl::span<const size_t> state_rep,
      gsl::span<const size_t> dimension_sizes, gsl::span<const size_t> dimension_capacities,
      size_t n_repetitions, size_t diluted_spacing, size_t n_instances)
      : state_rep_(state_rep.begin(), state_rep.end()),
        n_words_(state_rep.size()),
        extended_dimension_sizes_(InitExtendedDimensionSizes(dimension_sizes)),
        dimension_capacities_(dimension_capacities.begin(), dimension_capacities.end()),
        total_period_(std::accumulate(
            dimension_capacities.begin(), dimension_capacities.end(), 1, std::multiplies<>())),
        n_total_bits_(std::accumulate(state_rep.begin(), state_rep.end(), size_t(0))),
        extended_dimensions_total_size_(std::accumulate(
            extended_dimension_sizes_.begin(), extended_dimension_sizes_.end(), 1,
            std::multiplies<>())),
        n_repetitions_(n_repetitions),
        diluted_spacing_(diluted_spacing),
        n_instances_(n_instances),
        intermediate_column_(ctx.GetVirtualColumn(name + "/reshaped_intermediate")),
        final_column_(ctx.GetVirtualColumn(name + "/final_reshaped_input")),
        cumulative_sum_column_(ctx.GetVirtualColumn(name + "/cumulative_sum")) {
    for (size_t word : state_rep_) {
      ASSERT_RELEASE(
          word <= FieldElementT::FieldSize().Log2Floor(),
          "Word is too large to represent in a single field element.");
    }
    ASSERT_RELEASE(
        n_total_bits_ ==
            std::accumulate(
                dimension_sizes.begin(), dimension_sizes.end(), size_t(1), std::multiplies<>()),
        "Inconsistent n_total_bits_.");
    ASSERT_RELEASE(
        extended_dimension_sizes_.size() == dimension_capacities_.size(),
        "Inconsistent dimensions.");
  }

  /*
    Computes the row of index m in a virtual column with dimensions given by dimension_capacities_
    and extended_dimension_sizes_. The least significant part of m corresponds to the first
    dimension and the most significant part of m corresponds to the last dimension.
  */
  size_t RowIndex(size_t m) const;

  /*
    Writes the trace for one instance of the component.
    input is the list of field elements as they appear in the input column.
    One instance includes n_instances_ * n_repetitions_ * n_words_ field elements.
    Writes also the output columns in diluted_columns.
  */
  void WriteTrace(
      gsl::span<const FieldElementT> input,
      gsl::span<const TableCheckCellView<FieldElementT>> diluted_columns, uint64_t component_index,
      gsl::span<const gsl::span<FieldElementT>> trace) const;

 private:
  static std::vector<size_t> InitExtendedDimensionSizes(gsl::span<const size_t> dimension_sizes) {
    std::vector<size_t> extended_dimension_sizes(
        dimension_sizes.begin(), dimension_sizes.end() - 1);
    extended_dimension_sizes.push_back(*(dimension_sizes.end() - 1) + 1);
    return extended_dimension_sizes;
  }

  /*
    The division of the input bits to field elements.
  */
  const std::vector<size_t> state_rep_;

  /*
    The period of the input virtual column.
  */
  const size_t n_words_;

  /*
    The dimensions to which the bits are parsed.
  */
  const std::vector<size_t> extended_dimension_sizes_;
  const std::vector<size_t> dimension_capacities_;

  /*
    The total period of the parsed output.
  */
  const size_t total_period_;

  /*
    The total amount of bits in the input format.
  */
  const size_t n_total_bits_;

  /*
    The product of the dimensions of cumulative_sum (which are equal to dimension_sizes except that
    the last dimension is larger by 1).
  */
  const size_t extended_dimensions_total_size_;

  /*
    The number of repetitions.
  */
  const size_t n_repetitions_;

  /*
    The space between representation bits.
  */
  const size_t diluted_spacing_;

  /*
    The number of representation bits.
  */
  const size_t n_instances_;

  /*
    A virtual column for the intermediate shape inputs.
  */
  const VirtualColumn intermediate_column_;

  /*
    A virtual column for the final shape inputs.
  */
  const VirtualColumn final_column_;

  /*
    A virtual column for the cumulative sum.
  */
  const VirtualColumn cumulative_sum_column_;
};

}  // namespace starkware

#include "starkware/air/components/parse_to_diluted/parse_to_diluted.inl"

#endif  // STARKWARE_AIR_COMPONENTS_PARSE_TO_DILUTED_PARSE_TO_DILUTED_H_
