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

#include "starkware/algebra/polymorphic/field_element.h"

#include "starkware/algebra/polymorphic/field.h"

namespace starkware {

FieldElement::FieldElement(const FieldElement& other) : wrapper_(other.wrapper_->Clone()) {}

FieldElement& FieldElement::operator=(const FieldElement& other) & {
  if (this != &other) {
    wrapper_ = other.wrapper_->Clone();
  }

  return *this;
}

Field FieldElement::GetField() const { return wrapper_->GetField(); }

FieldElement FieldElement::operator+(const FieldElement& other) const { return *wrapper_ + other; }

FieldElement FieldElement::operator-(const FieldElement& other) const { return *wrapper_ - other; }

FieldElement FieldElement::operator-() const { return -*wrapper_; }

FieldElement FieldElement::operator*(const FieldElement& other) const { return *wrapper_ * other; }

FieldElement FieldElement::operator/(const FieldElement& other) const { return *wrapper_ / other; }

FieldElement FieldElement::Inverse() const { return wrapper_->Inverse(); }

FieldElement FieldElement::Pow(uint64_t exp) const { return wrapper_->Pow(exp); }

void FieldElement::ToBytes(gsl::span<std::byte> span_out, bool use_big_endian) const {
  wrapper_->ToBytes(span_out, use_big_endian);
}

bool FieldElement::operator==(const FieldElement& other) const { return wrapper_->Equals(other); }

bool FieldElement::operator!=(const FieldElement& other) const { return !(*this == other); }

size_t FieldElement::SizeInBytes() const { return wrapper_->SizeInBytes(); }

std::string FieldElement::ToString() const { return wrapper_->ToString(); }

std::ostream& operator<<(std::ostream& out, const FieldElement& field_element) {
  return out << field_element.ToString();
}

}  // namespace starkware
