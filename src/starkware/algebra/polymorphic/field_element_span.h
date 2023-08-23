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

/*
  FieldElementSpan is a polymorphic version of gsl::span<FieldElementT> for any field element type.
  Similarly, ConstFieldElementSpan is the polymorphic version of gsl::span<const FieldElementT>.

  Both class should be though of as "pointers" - For example, passing 'const FieldElementSpan&' to a
  function doesn't mean the function cannot change the pointee elements. Rather, it says that the
  "pointer" is constant. To say that the function will not change the field elements, pass
  'const ConstFieldElementSpan&'.

  Note that one polymorphic class that handles both 'FieldElementT' and 'const FieldElementT' is not
  enough, as we need to be able to distinguish between the two at compile time.
*/

#ifndef STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_ELEMENT_SPAN_H_
#define STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_ELEMENT_SPAN_H_

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/polymorphic/field.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

class FieldElementVector;
class ConstFieldElementSpan;

template <bool IsConst, typename Subclass>
class FieldElementSpanImpl {
 public:
  FieldElementSpanImpl(const FieldElementSpanImpl& other);

  FieldElementSpanImpl& operator=(const FieldElementSpanImpl& other);

  ~FieldElementSpanImpl() = default;
  FieldElementSpanImpl(FieldElementSpanImpl&& other) noexcept = default;
  FieldElementSpanImpl& operator=(FieldElementSpanImpl&& other) noexcept = default;

  Subclass SubSpan(size_t offset) const;

  Subclass SubSpan(size_t offset, size_t count) const;

  /*
    Returns the size of the span.
  */
  size_t Size() const;

  /*
    Returns the value of the span at a given index.
  */
  FieldElement operator[](size_t index) const;

  /*
    Returns the underlying field as an instance of the Field class.
  */
  Field GetField() const;

  /*
    Returns true if the two spans are equals.
    This function fails if other is over a different field.
  */
  bool operator==(const Subclass& other) const;

  bool operator!=(const Subclass& other) const { return !(*this == other); }

 protected:
  class WrapperBase;

  template <typename FieldElementT>
  class Wrapper;

  explicit FieldElementSpanImpl(std::unique_ptr<WrapperBase> wrapper)
      : wrapper_(std::move(wrapper)) {}

  std::unique_ptr<WrapperBase> wrapper_;
};

class FieldElementSpan : public FieldElementSpanImpl<false, FieldElementSpan> {
 public:
  /*
    Constructor from gsl::span.
  */
  template <typename FieldElementT>
  explicit FieldElementSpan(gsl::span<FieldElementT> value);

  FieldElementSpan(FieldElementVector& vec);  // NOLINT: implicit cast, allow non-const reference.

  /*
    Sets value at given index.
  */
  void Set(size_t index, const FieldElement& value) const;

  /*
    Copies the content of other to this.
  */
  void CopyDataFrom(const ConstFieldElementSpan& other) const;

  /*
    Asserts that the underlying type is FieldElementT, and returns the underlying value.
  */
  template <typename FieldElementT>
  gsl::span<FieldElementT> As() const;
};

class ConstFieldElementSpan : public FieldElementSpanImpl<true, ConstFieldElementSpan> {
 public:
  /*
    Constructor from gsl::span.
  */
  template <typename FieldElementT>
  explicit ConstFieldElementSpan(gsl::span<const FieldElementT> value);

  ConstFieldElementSpan(const FieldElementVector& vec);  // NOLINT: implicit cast.

  /*
    Asserts that the underlying type is FieldElementT, and returns the underlying value.
  */
  template <typename FieldElementT>
  gsl::span<const FieldElementT> As() const;
};

template <bool IsConst, typename Subclass>
std::ostream& operator<<(std::ostream& out, const FieldElementSpanImpl<IsConst, Subclass>& span);

}  // namespace starkware

#include "starkware/algebra/polymorphic/field_element_span.inl"

#endif  // STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_ELEMENT_SPAN_H_
