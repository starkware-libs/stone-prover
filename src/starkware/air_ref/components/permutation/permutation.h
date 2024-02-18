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

#ifndef STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_H_
#define STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_H_

#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/components/virtual_column.h"
#include "starkware/algebra/field_operations.h"

namespace starkware {

template <typename FieldElementT>
class PermutationComponent {
 public:
  PermutationComponent(const std::string& name, size_t n_series, const TraceGenerationContext& ctx);

  /*
    Given two columns original and perm of the existing trace and the interaction element, fills up
    the virtual column of the given interaction trace.
    expected_public_memory_prod is the expected value of the public memory product which is the last
    element in the cum_prod column of the interaction trace.
  */
  void WriteInteractionTrace(
      gsl::span<const gsl::span<const FieldElementT>> origs,
      gsl::span<const gsl::span<const FieldElementT>> perms, FieldElementT interaction_elm,
      gsl::span<const gsl::span<FieldElementT>> interaction_trace,
      const FieldElementT& expected_public_memory_prod) const;

 private:
  /*
    The virtual column in which the component is located.
  */
  const size_t n_series_;
  std::vector<VirtualColumn> cum_prod_cols_;
};

}  // namespace starkware

#include "starkware/air/components/permutation/permutation.inl"

#endif  // STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_H_
