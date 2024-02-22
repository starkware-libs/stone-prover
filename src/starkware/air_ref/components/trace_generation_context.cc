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

#include "starkware/air/components/trace_generation_context.h"

#include "starkware/error_handling/error_handling.h"

namespace starkware {

void TraceGenerationContext::AddVirtualColumn(
    const std::string& name, const VirtualColumn& virtual_column) {
  virtual_columns_embedding_.emplace(name, virtual_column);
}

const VirtualColumn& TraceGenerationContext::GetVirtualColumn(const std::string& name) const {
  const auto result = virtual_columns_embedding_.find(name);
  ASSERT_RELEASE(
      result != virtual_columns_embedding_.end(), "Virtual column '" + name + "' not found");
  return result->second;
}

void TraceGenerationContext::AddPeriodicColumn(
    const std::string& name, const VirtualColumn& periodic_column) {
  periodic_columns_embedding_.emplace(name, periodic_column);
}

const VirtualColumn& TraceGenerationContext::GetPeriodicColumn(const std::string& name) const {
  const auto result = periodic_columns_embedding_.find(name);
  ASSERT_RELEASE(
      result != periodic_columns_embedding_.end(), "Periodic column '" + name + "' not found");
  return result->second;
}

}  // namespace starkware
