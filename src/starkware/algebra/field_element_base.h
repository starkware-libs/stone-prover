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

#ifndef STARKWARE_ALGEBRA_FIELD_ELEMENT_BASE_H_
#define STARKWARE_ALGEBRA_FIELD_ELEMENT_BASE_H_

#include <iostream>
#include <limits>
#include <vector>

#include "starkware/utils/attributes.h"

namespace starkware {

/*
  A non-virtual base class for FieldElement-s (using CRTP). This class provides some common
  field-agnostic functionality.
  For CRTP, see https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern.

  To define a field element that has the functionality provided by this class, write:
    class MyFieldElement : public FieldElementBase<MyFieldElement> {
      ...
    };
*/
template <typename Derived>
class FieldElementBase {
 public:
  Derived& operator+=(const Derived& other);
  ALWAYS_INLINE Derived& operator*=(const Derived& other);
  Derived operator/(const Derived& other) const;
  constexpr bool operator!=(const Derived& other) const;

  static std::vector<Derived> UninitializedVector(size_t size) {
#ifdef NDEBUG
    return std::vector<Derived>(size);  // for faster memory allocation.
#else
    return std::vector<Derived>(size, Derived::Uninitialized());
#endif
  }

  /*
    Implements the FFT radix2 butterfly.

    The function takes both in and out pointers as we want the first FFT layer to copy the
    data.
    The following layers work in-place so we allow in{N} to alias out{N} as we want to do most of
    the FFT layer in place.

    The FftButterfly may be done in non standard representation so FftNormalize needs to be called
    before the output is exposed to other users.
  */
  static void FftButterfly(
      const Derived& in1, const Derived& in2, const Derived& twiddle_factor, Derived* out1,
      Derived* out2);

  /*
    Normalizes the output of FftButterfly to non-redundent representation.
  */
  static void FftNormalize(Derived* val);

  constexpr const Derived& AsDerived() const { return static_cast<const Derived&>(*this); }
  constexpr Derived& AsDerived() { return static_cast<Derived&>(*this); }

  /*
    Relevant in case the Stark protocol uses an extension field. In that case, returns true iff the
    element is a base field element. Otherwise, always returns true.
  */
  bool InBaseField() const { return true; }

  /*
    Converts an int64 to a FieldElement.
  */
  static Derived FromInt(int64_t num) {
    if (num >= 0) {
      return Derived::FromUint(num);
    }
    return -Derived::FromUint(-num);
  }

  /*
    Relevant in case the Stark protocol uses an extension field. In that case, returns the generator
    of the base field. Otherwise, returns the generator of the current field.
  */
  static Derived GetBaseGenerator() { return Derived::Generator(); }
};

/*
  A constant with the value true if FieldElementT is a field.
  For example, kIsFieldElement<TestFieldElement> == true and kIsFieldElement<int> == false.
*/
template <typename FieldElementT>
constexpr bool kIsFieldElement =
    std::is_base_of<FieldElementBase<FieldElementT>, FieldElementT>::value;

template <typename FieldElementT>
std::enable_if_t<kIsFieldElement<FieldElementT>, std::ostream&> operator<<(
    std::ostream& out, const FieldElementT& element);

}  // namespace starkware

#include "starkware/algebra/field_element_base.inl"

#endif  // STARKWARE_ALGEBRA_FIELD_ELEMENT_BASE_H_
