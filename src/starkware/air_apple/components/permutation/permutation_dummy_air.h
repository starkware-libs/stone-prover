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

#ifndef STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_DUMMY_AIR_H_
#define STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_DUMMY_AIR_H_

#include <memory>
#include <vector>

#include "starkware/air/air.h"
#include "starkware/air/components/permutation/multi_column_permutation.h"
#include "starkware/air/components/permutation/permutation_dummy_air_definition.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/air/trace.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/composition_polynomial/composition_polynomial.h"
#include "starkware/math/math.h"
#include "starkware/randomness/prng.h"

namespace starkware {

template <typename FieldElementT, int LayoutId>
class PermutationDummyAir : public PermutationDummyAirDefinition<FieldElementT, LayoutId> {
 public:
  explicit PermutationDummyAir(uint64_t trace_length, Prng* prng)
      : PermutationDummyAirDefinition<FieldElementT, LayoutId>(trace_length), prng_(prng) {}

  std::unique_ptr<const Air> WithInteractionElements(
      const FieldElementVector& interaction_elms) const override {
    const auto& interaction_elms_vec = interaction_elms.As<FieldElementT>();
    return WithInteractionElementsImpl(interaction_elms_vec);
  }

  std::unique_ptr<const PermutationDummyAir<FieldElementT, LayoutId>> WithInteractionElementsImpl(
      const gsl::span<const FieldElementT>& interaction_elms) const;

  /*
    Creates two columns - one of random values and the other one that is a permutation of these
    values.
  */
  Trace GetTrace() const;

  /*
    Given two columns of the existing trace, generates an interaction trace with a single column
    using values from these columns and the interaction element.
  */
  Trace GetInteractionTrace(
      gsl::span<const gsl::span<const FieldElementT>> originals,
      gsl::span<const gsl::span<const FieldElementT>> perms) const;

 private:
  Prng* prng_;
};

}  // namespace starkware

#include "starkware/air/components/permutation/permutation_dummy_air.inl"

#endif  // STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_DUMMY_AIR_H_
