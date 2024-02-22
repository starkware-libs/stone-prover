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

#ifndef STARKWARE_AIR_COMPONENTS_HASH_HASH_FACTORY_H_
#define STARKWARE_AIR_COMPONENTS_HASH_HASH_FACTORY_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/hash/hash_component.h"
#include "starkware/air/components/trace_generation_context.h"
#include "starkware/crypt_tools/hash_context/hash_context.h"

namespace starkware {

/*
  An abstract base class that specifies the interface of Hash Factories.
  Every hash component should implement its own HashFactory, which extends this class.
  This allows for true polymorphism when using different hash components.
*/
template <typename FieldElementT>
class HashFactory {
 public:
  explicit HashFactory(std::string name) : name_(std::move(name)) {}
  virtual ~HashFactory() = default;

  /*
    The factory method.
    Creates a new hash component, as implemented by the derived class.
  */
  virtual std::unique_ptr<HashComponent<FieldElementT>> CreateComponent(
      const std::string& name, const TraceGenerationContext& ctx) const = 0;

  /*
    Prepares values for the periodic columns required for the hash.
  */
  virtual std::map<const std::string, std::vector<FieldElementT>> ComputePeriodicColumnValues()
      const = 0;

 protected:
  const std::string name_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_COMPONENTS_HASH_HASH_FACTORY_H_
