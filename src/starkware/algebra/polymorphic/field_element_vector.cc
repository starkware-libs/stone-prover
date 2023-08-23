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

#include "starkware/algebra/polymorphic/field_element_vector.h"

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/utils/invoke_template_version.h"

namespace starkware {

size_t FieldElementVector::Size() const { return wrapper_->Size(); }
FieldElement FieldElementVector::operator[](size_t index) const { return (*wrapper_)[index]; }
void FieldElementVector::Set(size_t index, const FieldElement& elt) {
  return wrapper_->Set(index, elt);
}
void FieldElementVector::PushBack(const FieldElement& elt) { return wrapper_->PushBack(elt); }
void FieldElementVector::Reserve(size_t size) { return wrapper_->Reserve(size); }
Field FieldElementVector::GetField() const { return wrapper_->GetField(); }
bool FieldElementVector::operator==(const FieldElementVector& other) const {
  return *wrapper_ == *other.wrapper_;
}
FieldElementSpan FieldElementVector::AsSpan() { return wrapper_->AsSpan(); }
ConstFieldElementSpan FieldElementVector::AsSpan() const {
  return static_cast<const WrapperBase&>(*wrapper_).AsSpan();
}

FieldElementVector FieldElementVector::MakeUninitialized(const Field& field, size_t size) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) {
        using FieldElementT = typename decltype(field_tag)::type;
        return MakeUninitialized<FieldElementT>(size);
      },
      field);
}

FieldElementVector FieldElementVector::Make(size_t size, const FieldElement& value) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) {
        using FieldElementT = typename decltype(field_tag)::type;
        return Make<FieldElementT>(std::vector<FieldElementT>(size, value.As<FieldElementT>()));
      },
      value.GetField());
}

FieldElementVector FieldElementVector::CopyFrom(const ConstFieldElementSpan& values) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) {
        using FieldElementT = typename decltype(field_tag)::type;
        return FieldElementVector::CopyFrom(values.template As<FieldElementT>());
      },
      values.GetField());
}

void FieldElementVector::LinearCombination(
    const ConstFieldElementSpan& coefficients, const std::vector<ConstFieldElementSpan>& vectors,
    const FieldElementSpan& output) {
  const Field field = coefficients.GetField();
  for (const auto& vec : vectors) {
    ASSERT_RELEASE(vec.GetField() == field, "Vectors must be over same field.");
  }
  ASSERT_RELEASE(output.GetField() == field, "Output must be over same field as input.");
  InvokeFieldTemplateVersion(
      [&](auto field_tag) {
        using FieldElementT = typename decltype(field_tag)::type;
        std::vector<gsl::span<const FieldElementT>> vectors_as;
        vectors_as.reserve(vectors.size());
        for (auto& v : vectors) {
          vectors_as.push_back(v.As<FieldElementT>());
        }
        const gsl::span<const FieldElementT> coefficients_as = coefficients.As<FieldElementT>();
        gsl::span<FieldElementT> output_as = output.As<FieldElementT>();
        ::starkware::LinearCombination(
            coefficients_as, gsl::span<const gsl::span<const FieldElementT>>(vectors_as),
            output_as);
      },
      field);
}

std::ostream& operator<<(std::ostream& out, const FieldElementVector& vec) {
  for (size_t i = 0; i < vec.Size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << vec[i];
  }
  return out;
}

}  // namespace starkware
