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

#include "starkware/algebra/fields/prime_field_element.h"

namespace starkware {

template <int NBits, int Index>
void PrimeFieldElement<NBits, Index>::ToBytes(
    gsl::span<std::byte> span_out, bool use_big_endian) const {
  return value_.ToBytes(span_out, use_big_endian);
}

template <int NBits, int Index>
void PrimeFieldElement<NBits, Index>::ToBytesStandardForm(
    gsl::span<std::byte> span_out, bool use_big_endian) const {
  ToStandardForm().ToBytes(span_out, use_big_endian);
}

template <int NBits, int Index>
PrimeFieldElement<NBits, Index> PrimeFieldElement<NBits, Index>::FromBytes(
    gsl::span<const std::byte> bytes, bool use_big_endian) {
  ValueType element = ValueType::FromBytes(bytes, use_big_endian);
  ASSERT_RELEASE(element < GetModulus(), "The input must be smaller than the field prime.");
  return PrimeFieldElement(element);
}

template <int NBits, int Index>
PrimeFieldElement<NBits, Index> PrimeFieldElement<NBits, Index>::FromString(const std::string& s) {
  return PrimeFieldElement<NBits, Index>::FromBigInt(ValueType::FromString(s));
}

template <int NBits, int Index>
PrimeFieldElement<NBits, Index> PrimeFieldElement<NBits, Index>::RandomElement(PrngBase* prng) {
  constexpr ValueType kBound = GetMaxDivisible();

  ValueType element = ValueType::RandomBigInt(prng);
  // Required to enforce uniformity.
  while (element >= kBound) {
    element = ValueType::RandomBigInt(prng);
  }

  return PrimeFieldElement(element.Div(GetModulus()).second);
}

template class PrimeFieldElement<252, 0>;
template class PrimeFieldElement<254, 1>;
template class PrimeFieldElement<254, 2>;
template class PrimeFieldElement<252, 3>;
template class PrimeFieldElement<255, 4>;
template class PrimeFieldElement<124, 5>;

}  // namespace starkware
