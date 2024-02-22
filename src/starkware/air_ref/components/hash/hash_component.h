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

#ifndef STARKWARE_AIR_COMPONENTS_HASH_HASH_COMPONENT_H_
#define STARKWARE_AIR_COMPONENTS_HASH_HASH_COMPONENT_H_

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/crypt_tools/hash_context/hash_context.h"

namespace starkware {

/*
  An abstract class that specifies the interface of Hash Components.
  All classes that use hash components should be using this base class, all hash component should
  be extending it. This gives extra flexibility and allows to easily swap different hash components.
*/
template <typename FieldElementT>
class HashComponent {
 public:
  virtual ~HashComponent() = default;

  /*
    Writes the trace for one instance of the component.
    Returns the result of the hash on the given inputs.
    component_index is the index of the component instance.
  */
  virtual FieldElementT WriteTrace(
      gsl::span<const FieldElementT> inputs, uint64_t component_index,
      gsl::span<const gsl::span<FieldElementT>> trace) const = 0;

  /*
    Returns the context / configuration of this instance.
  */
  virtual const HashContext<FieldElementT>& GetHashContext() const = 0;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_COMPONENTS_HASH_HASH_COMPONENT_H_
