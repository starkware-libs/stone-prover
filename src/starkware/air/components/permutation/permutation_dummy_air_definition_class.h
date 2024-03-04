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

#ifndef STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_DUMMY_AIR_DEFINITION_CLASS_H_
#define STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_DUMMY_AIR_DEFINITION_CLASS_H_

namespace starkware {

template <typename FieldElementT, int LayoutId = 0>
class PermutationDummyAirDefinition {
  // Workaround for static_assert(false).
  static_assert(
      sizeof(FieldElementT) == 0,
      "PermutationDummyAirDefinition is not implemented for the given field and layout.");
};

}  // namespace starkware

#endif  // STARKWARE_AIR_COMPONENTS_PERMUTATION_PERMUTATION_DUMMY_AIR_DEFINITION_CLASS_H_
