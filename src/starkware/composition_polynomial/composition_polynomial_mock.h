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

#ifndef STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_MOCK_H_
#define STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_MOCK_H_

#include <memory>
#include <vector>

#include "gmock/gmock.h"

#include "starkware/composition_polynomial/composition_polynomial.h"

namespace starkware {

class CompositionPolynomialMock : public CompositionPolynomial {
 public:
  MOCK_CONST_METHOD2(EvalAtPoint, FieldElement(const FieldElement&, const ConstFieldElementSpan&));
  MOCK_CONST_METHOD4(
      EvalOnCosetBitReversedOutput, void(
                                        const FieldElement&, gsl::span<const ConstFieldElementSpan>,
                                        const FieldElementSpan&, uint64_t));
  MOCK_CONST_METHOD0(GetDegreeBound, uint64_t());
};

}  // namespace starkware

#endif  // STARKWARE_COMPOSITION_POLYNOMIAL_COMPOSITION_POLYNOMIAL_MOCK_H_
