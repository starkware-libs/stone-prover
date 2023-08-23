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

#include "starkware/algebra/domains/multiplicative_group.h"

#include <utility>

#include "starkware/algebra/utils/invoke_template_version.h"
#include "starkware/math/math.h"

namespace starkware {

MultiplicativeGroup MultiplicativeGroup::MakeGroup(const size_t size, const Field& field) {
  auto get_generator = [&](auto field_tag) {
    using FieldElementT = typename decltype(field_tag)::type;
    return FieldElement(GetSubGroupGenerator<FieldElementT>(size));
  };
  return MultiplicativeGroup(size, InvokeFieldTemplateVersion(get_generator, field));
}

MultiplicativeGroup::MultiplicativeGroup(size_t size, FieldElement generator)
    : size_(size), generator_(std::move(generator)) {}

}  // namespace starkware
