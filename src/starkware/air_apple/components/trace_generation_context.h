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

#ifndef STARKWARE_AIR_COMPONENTS_TRACE_GENERATION_CONTEXT_H_
#define STARKWARE_AIR_COMPONENTS_TRACE_GENERATION_CONTEXT_H_

#include <any>
#include <map>
#include <string>

#include "starkware/air/components/virtual_column.h"

namespace starkware {

/*
  Context used for trace generation. Should be instantiated in the Air once, and then passed to the
  components.
*/
class TraceGenerationContext {
 public:
  /*
    Adds a virtual column. Should only be called by GetTraceGenerationContext(). These calls are
    auto-generated.
  */
  void AddVirtualColumn(const std::string& name, const VirtualColumn& virtual_column);

  /*
    Gets a virtual column by name.
  */
  const VirtualColumn& GetVirtualColumn(const std::string& name) const;

  /*
    Adds a periodic column. Should only be called by GetTraceGenerationContext(). These calls are
    auto-generated.
  */
  void AddPeriodicColumn(const std::string& name, const VirtualColumn& periodic_column);

  /*
    Gets a periodic column by name.
  */
  const VirtualColumn& GetPeriodicColumn(const std::string& name) const;

  /*
    Adds a generic object. Should only be called by GetTraceGenerationContext(). These calls are
    auto-generated.
  */
  template <typename T>
  void AddObject(const std::string& name, const T& object) {
    objects_.emplace(name, std::make_any<T>(object));
  }

  /*
    Gets an object by name and type.
  */
  template <typename T>
  T GetObject(const std::string& name) const {
    const auto result = objects_.find(name);
    ASSERT_RELEASE(result != objects_.end(), "Object '" + name + "' not found");
    ASSERT_RELEASE(
        result->second.type() == typeid(T), "Object '" + name + "' is of the wrong type.");
    return std::any_cast<T>(result->second);
  }

 private:
  std::map<const std::string, VirtualColumn> virtual_columns_embedding_;
  std::map<const std::string, VirtualColumn> periodic_columns_embedding_;
  /*
    A map from object names created by code generation to the respective objects (using std::any to
    wrap arbitrary types).
  */
  std::map<const std::string, std::any> objects_;
};

}  // namespace starkware

#endif  // STARKWARE_AIR_COMPONENTS_TRACE_GENERATION_CONTEXT_H_
