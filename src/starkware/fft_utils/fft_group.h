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


#ifndef STARKWARE_FFT_UTILS_FFT_GROUP_H_
#define STARKWARE_FFT_UTILS_FFT_GROUP_H_

#include "starkware/algebra/field_operations.h"

namespace starkware {

template <typename T>
class FftMultiplicativeGroup {
 public:
  using FieldElementT = T;
  static FieldElementT GroupUnit() { return FieldElementT::One(); }

  static FieldElementT GroupOperation(const FieldElementT& a, const FieldElementT& b) {
    return a * b;
  }

  static FieldElementT GroupOperationInverse(const FieldElementT& a) { return a.Inverse(); }

  static void IfftButterfly(
      const FieldElementT& in1, const FieldElementT& in2, const FieldElementT& twiddle_factor,
      FieldElementT* out1, FieldElementT* out2);
};

}  // namespace starkware

#include "starkware/fft_utils/fft_group.inl"

#endif  // STARKWARE_FFT_UTILS_FFT_GROUP_H_
