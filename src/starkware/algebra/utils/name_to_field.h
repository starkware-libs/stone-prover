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

#ifndef STARKWARE_ALGEBRA_UTILS_NAME_TO_FIELD_H_
#define STARKWARE_ALGEBRA_UTILS_NAME_TO_FIELD_H_

#include <optional>
#include <string>

#include "starkware/algebra/polymorphic/field.h"

namespace starkware {

std::optional<Field> NameToField(const std::string& field_name);

}  // namespace starkware

#endif  // STARKWARE_ALGEBRA_UTILS_NAME_TO_FIELD_H_
