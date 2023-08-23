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

#include "starkware/algebra/utils/name_to_field.h"

#include <optional>
#include <string>

#include "glog/logging.h"

#include "starkware/algebra/fields/extension_field_element.h"
#include "starkware/algebra/fields/long_field_element.h"
#include "starkware/algebra/fields/prime_field_element.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/polymorphic/field.h"

namespace starkware {

std::optional<Field> NameToField(const std::string& field_name) {
  if (field_name == "TestField") {
    return Field::Create<TestFieldElement>();
  }
  if (field_name == "PrimeField0") {
    return Field::Create<PrimeFieldElement<252, 0>>();
  }
  if (field_name == "PrimeField1") {
    return Field::Create<PrimeFieldElement<254, 1>>();
  }
  if (field_name == "PrimeField2") {
    return Field::Create<PrimeFieldElement<254, 2>>();
  }
  if (field_name == "PrimeField3") {
    return Field::Create<PrimeFieldElement<252, 3>>();
  }
  if (field_name == "PrimeField4") {
    return Field::Create<PrimeFieldElement<255, 4>>();
  }
  if (field_name == "LongField") {
    return Field::Create<LongFieldElement>();
  }
  if (field_name == "ExtensionLongField") {
    return Field::Create<ExtensionFieldElement<LongFieldElement>>();
  }
  if (field_name == "ExtensionTestField") {
    return Field::Create<ExtensionFieldElement<TestFieldElement>>();
  }
  if (field_name == "ExtensionPrimeField0") {
    return Field::Create<ExtensionFieldElement<PrimeFieldElement<252, 0>>>();
  }

  LOG(ERROR) << "Invalid field name: " << field_name;
  return std::nullopt;
}

}  // namespace starkware
