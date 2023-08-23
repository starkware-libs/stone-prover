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

#include <optional>
#include <string>

#include "glog/logging.h"

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

template <typename FieldElementT>
void TableCheckCell<FieldElementT>::WriteTrace(
    const uint64_t index, const uint64_t value,
    const gsl::span<const gsl::span<FieldElementT>> trace) {
  std::unique_lock lock(*write_trace_lock_);
  ASSERT_RELEASE(
      is_initialized_[index] == false,
      "Table check unit " + std::to_string(index) + " was already written.");
  is_initialized_[index] = true;
  values_[index] = value;
  vc_.SetCell(trace, index, FieldElementT::FromUint(value));
}

}  // namespace starkware
