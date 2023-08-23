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

namespace starkware {

template <typename FieldElementT>
void FftMultiplicativeGroup<FieldElementT>::IfftButterfly(
    const FieldElementT& in1, const FieldElementT& in2, const FieldElementT& twiddle_factor,
    FieldElementT* out1, FieldElementT* out2) {
  FieldElementT l = in1;
  FieldElementT r = in2;

  *out1 = l + r;
  *out2 = (l - r) * twiddle_factor;
}

}  // namespace starkware
