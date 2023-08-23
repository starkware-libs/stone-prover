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

#include "starkware/algebra/field_operations.h"

namespace starkware {

template <typename FieldElementT>
std::enable_if_t<kIsFieldElement<FieldElementT>, std::ostream&> operator<<(
    std::ostream& out, const FieldElementT& element) {
  return out << element.AsDerived().ToString();
}

template <typename Derived>
Derived& FieldElementBase<Derived>::operator+=(const Derived& other) {
  return AsDerived() = AsDerived() + other;
}

template <typename Derived>
Derived& FieldElementBase<Derived>::operator*=(const Derived& other) {
  return AsDerived() = AsDerived() * other;
}

template <typename Derived>
Derived FieldElementBase<Derived>::operator/(const Derived& other) const {
  return AsDerived() * other.Inverse();
}

template <typename Derived>
constexpr bool FieldElementBase<Derived>::operator!=(const Derived& other) const {
  return !(AsDerived() == other);
}

template <typename Derived>
void FieldElementBase<Derived>::FftButterfly(
    const Derived& in1, const Derived& in2, const Derived& twiddle_factor, Derived* out1,
    Derived* out2) {
  const Derived mul_res = in2 * twiddle_factor;

  // We write out2 first because out1 may alias in1.
  *out2 = in1 - mul_res;
  *out1 = in1 + mul_res;
}

template <typename Derived>
void FieldElementBase<Derived>::FftNormalize(Derived* /*val*/) {}

}  // namespace starkware
