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

#ifndef STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_ELEMENT_VECTOR_H_
#define STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_ELEMENT_VECTOR_H_

#include <memory>
#include <utility>
#include <vector>

#include "starkware/algebra/polymorphic/field.h"
#include "starkware/algebra/polymorphic/field_element.h"
#include "starkware/algebra/polymorphic/field_element_span.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

/*
  A class representing a vector of FieldElement-s of a common field.
  To create an instance call FieldElementVector::Make().
*/
class FieldElementVector {
 public:
  /*
    Returns the size of the vector.
  */
  size_t Size() const;

  /*
    Returns the value of the vector at a given index.
  */
  FieldElement operator[](size_t index) const;

  /*
    Same as operator[]. This is more readable when the object is given as a pointer.
  */
  FieldElement At(size_t index) const { return (*this)[index]; }

  /*
    Sets value at given index.
  */
  void Set(size_t index, const FieldElement& elt);

  void PushBack(const FieldElement& elt);

  /*
    Reserves space for the given number of field elements (same as std::vector<>::reserve).
  */
  void Reserve(size_t size);

  /*
    Returns the underlying field as an instance of the Field class.
  */
  Field GetField() const;

  /*
    Returns true if the two vectors are equals.
    This function fails if other is over a different field.
  */
  bool operator==(const FieldElementVector& other) const;

  bool operator!=(const FieldElementVector& other) const { return !(*this == other); }

  /*
    Asserts that the underlying type is FieldElementT, and returns the underlying value.
  */
  template <typename FieldElementT>
  const std::vector<FieldElementT>& As() const;

  /*
    Asserts that the underlying type is FieldElementT, and returns the underlying value.
  */
  template <typename FieldElementT>
  std::vector<FieldElementT>& As();

  /*
    Returns the polymorphic non-const FieldElementSpan for the entire vector.
  */
  FieldElementSpan AsSpan();
  ConstFieldElementSpan AsSpan() const;

  /*
    Creates an instance of FieldElementVector with the given underlying field type and the given
    size.
  */
  template <typename FieldElementT>
  static FieldElementVector MakeUninitialized(size_t size);

  /*
    Creates an empty instance of FieldElementVector with a given underlying field type.
  */
  template <typename FieldElementT>
  static FieldElementVector Make() {
    return MakeUninitialized<FieldElementT>(0);
  }

  /*
    Creates an instance of FieldElementVector with a given size.
  */
  static FieldElementVector MakeUninitialized(const Field& field, size_t size);

  /*
    Creates an empty instance of FieldElementVector.
  */
  static FieldElementVector Make(const Field& field) { return MakeUninitialized(field, 0); }

  /*
    Creates an instance of FieldElementVector with a given size, initialized with given value.
  */
  static FieldElementVector Make(size_t size, const FieldElement& value);

  /*
    Creates an instance of FieldElementVector with given values.
    For efficiency, call this function with an r-value:
    auto vec = MakeFieldElementVector(std::move(old_vec));
  */
  template <typename FieldElementT>
  static FieldElementVector Make(std::vector<FieldElementT>&& vec);

  /*
    Creates an instance of FieldElementVector, with the same values as the input.
  */
  template <typename FieldElementT>
  static FieldElementVector CopyFrom(const std::vector<FieldElementT>& vec);
  template <typename FieldElementT>
  static FieldElementVector CopyFrom(gsl::span<const FieldElementT> values);
  static FieldElementVector CopyFrom(const ConstFieldElementSpan& values);

  /*
    Returns a linear combination of given vectors and given coefficients and stores the result at
    output.
  */
  static void LinearCombination(
      const ConstFieldElementSpan& coefficients, const std::vector<ConstFieldElementSpan>& vectors,
      const FieldElementSpan& output);

 private:
  class WrapperBase;

  template <typename T>
  class Wrapper;

  std::unique_ptr<WrapperBase> wrapper_;

  explicit FieldElementVector(std::unique_ptr<WrapperBase>&& wrapper)
      : wrapper_(std::move(wrapper)) {}
};

std::ostream& operator<<(std::ostream& out, const FieldElementVector& vec);

}  // namespace starkware

#include "starkware/algebra/polymorphic/field_element_vector.inl"

#endif  // STARKWARE_ALGEBRA_POLYMORPHIC_FIELD_ELEMENT_VECTOR_H_
