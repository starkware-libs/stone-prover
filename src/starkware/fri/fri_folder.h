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

#ifndef STARKWARE_FRI_FRI_FOLDER_H_
#define STARKWARE_FRI_FRI_FOLDER_H_

#include <memory>

#include "starkware/algebra/polymorphic/field_element_vector.h"
#include "starkware/fft_utils/fft_bases.h"

namespace starkware {
namespace fri {
namespace details {

/*
  This class performs the "FRI formula", folding current layer into the next.
*/
class FriFolderBase {
 public:
  virtual ~FriFolderBase() = default;

  /*
    Computes the values of the next FRI layer given the values and domain of the current layer.
  */
  virtual FieldElementVector ComputeNextFriLayer(
      const FftDomainBase& domain, const ConstFieldElementSpan& values,
      const FieldElement& eval_point) const = 0;
  virtual void ComputeNextFriLayer(
      const FftDomainBase& domain, const ConstFieldElementSpan& values,
      const FieldElement& eval_point, const FieldElementSpan& output_layer) const = 0;

  /*
    Computes the value of a single element in the next FRI layer given two corresponding
    elements in the current layer.
  */
  virtual FieldElement NextLayerElementFromTwoPreviousLayerElements(
      const FieldElement& f_x, const FieldElement& f_minus_x, const FieldElement& eval_point,
      const FieldElement& x_inv) const = 0;
};

std::unique_ptr<FriFolderBase> FriFolderFromField(const Field& field);

}  // namespace details
}  // namespace fri
}  // namespace starkware

#endif  // STARKWARE_FRI_FRI_FOLDER_H_
