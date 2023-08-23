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

#include "starkware/algebra/polymorphic/field_element_span.h"

#include "starkware/algebra/polymorphic/field_element_vector.h"

namespace starkware {

template <bool IsConst, typename Subclass>
FieldElementSpanImpl<IsConst, Subclass>::FieldElementSpanImpl(const FieldElementSpanImpl& other)
    : wrapper_(other.wrapper_->Clone()) {}

template <bool IsConst, typename Subclass>
FieldElementSpanImpl<IsConst, Subclass>& FieldElementSpanImpl<IsConst, Subclass>::operator=(
    const FieldElementSpanImpl& other) {
  if (this != &other) {
    wrapper_ = other.wrapper_->Clone();
  }

  return *this;
}

template <bool IsConst, typename Subclass>
size_t FieldElementSpanImpl<IsConst, Subclass>::Size() const {
  return wrapper_->Size();
}

template <bool IsConst, typename Subclass>
FieldElement FieldElementSpanImpl<IsConst, Subclass>::operator[](size_t index) const {
  return (*wrapper_)[index];
}

template <bool IsConst, typename Subclass>
Field FieldElementSpanImpl<IsConst, Subclass>::GetField() const {
  return wrapper_->GetField();
}

template <bool IsConst, typename Subclass>
bool FieldElementSpanImpl<IsConst, Subclass>::operator==(const Subclass& other) const {
  return wrapper_->IsEqual(other);
}

template <bool IsConst, typename Subclass>
Subclass FieldElementSpanImpl<IsConst, Subclass>::SubSpan(size_t offset) const {
  return wrapper_->SubSpan(offset);
}

template <bool IsConst, typename Subclass>
Subclass FieldElementSpanImpl<IsConst, Subclass>::SubSpan(size_t offset, size_t count) const {
  return wrapper_->SubSpan(offset, count);
}

template class FieldElementSpanImpl<false, FieldElementSpan>;
template class FieldElementSpanImpl<true, ConstFieldElementSpan>;

FieldElementSpan::FieldElementSpan(FieldElementVector& vec)  // NOLINT: allow non-const reference.
    : FieldElementSpan(vec.AsSpan()) {}

ConstFieldElementSpan::ConstFieldElementSpan(const FieldElementVector& vec)
    : ConstFieldElementSpan(vec.AsSpan()) {}

void FieldElementSpan::Set(size_t index, const FieldElement& value) const {
  wrapper_->Set(index, value);
}

void FieldElementSpan::CopyDataFrom(const ConstFieldElementSpan& other) const {
  wrapper_->CopyDataFrom(other);
}

template <bool IsConst, typename Subclass>
std::ostream& operator<<(std::ostream& out, const FieldElementSpanImpl<IsConst, Subclass>& span) {
  for (size_t i = 0; i < span.Size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << span[i];
  }
  return out;
}

template std::ostream& operator<<(
    std::ostream& out, const FieldElementSpanImpl<true, ConstFieldElementSpan>& span);

template std::ostream& operator<<(
    std::ostream& out, const FieldElementSpanImpl<false, FieldElementSpan>& span);

}  // namespace starkware
