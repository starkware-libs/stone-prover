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

#include "starkware/algebra/utils/invoke_template_version.h"

#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/polymorphic/field_element_vector.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {
namespace {

using testing::ElementsAre;

template <typename T>
std::vector<T> AddVectorsImpl(const std::vector<T>& vec1, const std::vector<T>& vec2) {
  ASSERT_RELEASE(
      vec1.size() == vec2.size(), "sizes of vectors for point-wise addition do not match");

  std::vector<T> result;
  result.reserve(vec1.size());
  for (size_t i = 0; i < vec1.size(); i++) {
    result.push_back(vec1[i] + vec2[i]);
  }
  return result;
}

FieldElementVector AddVectors(const FieldElementVector& vec1, const FieldElementVector& vec2) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) {
        using T = typename decltype(field_tag)::type;
        return FieldElementVector::Make(AddVectorsImpl(vec1.As<T>(), vec2.As<T>()));
      },
      vec1.GetField());
}

TEST(InvokeFieldTemplateVersion, AddVectors) {
  auto vec1 = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(3), TestFieldElement::FromUint(5)});
  auto vec2 = FieldElementVector::Make<TestFieldElement>(
      {TestFieldElement::FromUint(1), TestFieldElement::FromUint(2)});
  EXPECT_THAT(
      AddVectors(vec1, vec2).As<TestFieldElement>(),
      ElementsAre(TestFieldElement::FromUint(4), TestFieldElement::FromUint(7)));
}

}  // namespace
}  // namespace starkware
