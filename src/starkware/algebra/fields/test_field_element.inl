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

#include "starkware/error_handling/error_handling.h"

namespace starkware {

inline TestFieldElement TestFieldElement::Inverse() const {
  // Run extended Euclidean algorithm. Implementation following pseudocode from:
  // https://en.wikipedia.org/wiki/Extended_Euclidean_algorithm#Computing_multiplicative_inverses_in_modular_structures.
  uint64_t t = 0, r = kModulus, newt = 1, newr = value_;
  while (newr != 0) {
    uint64_t quotient = r / newr;
    uint64_t tmp = t;
    t = newt;
    newt = (tmp + kModulus - ((quotient * newt) % kModulus)) % kModulus;
    tmp = r;
    r = newr;
    newr = tmp - quotient * newr;
  }
  ASSERT_RELEASE(r == 1, "Inverse operation failed - the GCD of value and modulus is not 1");
  ASSERT_DEBUG(t * value_ % kModulus == 1, "Inverse operation failed");
  return TestFieldElement(t);
}

}  // namespace starkware
