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

#include "starkware/algebra/fields/fraction_field_element.h"

namespace starkware {

template <typename FieldElementT>
FractionFieldElement<FieldElementT> FractionFieldElement<FieldElementT>::operator+(
    const FractionFieldElement<FieldElementT>& rhs) const {
  const auto num_value = this->numerator_ * rhs.denominator_ + this->denominator_ * rhs.numerator_;
  const auto denom_value = this->denominator_ * rhs.denominator_;
  return FractionFieldElement(num_value, denom_value);
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> FractionFieldElement<FieldElementT>::operator-(
    const FractionFieldElement<FieldElementT>& rhs) const {
  const auto num_value = this->numerator_ * rhs.denominator_ - this->denominator_ * rhs.numerator_;
  const auto denom_value = this->denominator_ * rhs.denominator_;
  return FractionFieldElement(num_value, denom_value);
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> FractionFieldElement<FieldElementT>::operator*(
    const FractionFieldElement<FieldElementT>& rhs) const {
  return FractionFieldElement(
      this->numerator_ * rhs.numerator_, this->denominator_ * rhs.denominator_);
}

template <typename FieldElementT>
bool FractionFieldElement<FieldElementT>::operator==(
    const FractionFieldElement<FieldElementT>& rhs) const {
  return this->numerator_ * rhs.denominator_ == this->denominator_ * rhs.numerator_;
}

template <typename FieldElementT>
FractionFieldElement<FieldElementT> FractionFieldElement<FieldElementT>::Inverse() const {
  ASSERT_RELEASE(this->numerator_ != FieldElementT::Zero(), "Zero does not have an inverse");
  return FractionFieldElement(denominator_, numerator_);
}

template <typename FieldElementT>
void FractionFieldElement<FieldElementT>::BatchToBaseFieldElement(
    gsl::span<const gsl::span<const FractionFieldElement<FieldElementT>>> input,
    gsl::span<const gsl::span<FieldElementT>> output) {
  size_t n_cols = input.size();

  // Create span of spans for input's denominators.
  std::vector<std::vector<FieldElementT>> denoms_input(n_cols);

  for (size_t i = 0; i < n_cols; ++i) {
    const size_t n_rows = input[i].size();
    denoms_input[i].reserve(n_rows);
    for (const FractionFieldElement& element : input[i]) {
      denoms_input[i].push_back(element.denominator_);
    }
  }

  // Run BatchInverse on denominators.
  BatchInverse<FieldElementT>(
      std::vector<gsl::span<const FieldElementT>>(denoms_input.begin(), denoms_input.end()),
      output);

  // Create batch BaseFieldElements.
  for (size_t i = 0; i < n_cols; ++i) {
    const size_t n_rows = input[i].size();
    for (size_t j = 0; j < n_rows; j++) {
      output[i][j] *= input[i][j].numerator_;
    }
  }
}

}  // namespace starkware
