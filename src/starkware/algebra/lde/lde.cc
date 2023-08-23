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

#include "starkware/algebra/lde/lde.h"

#include <memory>
#include <utility>

#include "starkware/algebra/domains/multiplicative_group.h"
#include "starkware/algebra/fft/fft_with_precompute.h"
#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/fields/test_field_element.h"
#include "starkware/algebra/lde/lde_manager_impl.h"
#include "starkware/algebra/polynomials.h"
#include "starkware/algebra/utils/invoke_template_version.h"
#include "starkware/utils/bit_reversal.h"

namespace starkware {

std::unique_ptr<LdeManager> MakeLdeManager(const FftBases& bases) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) -> std::unique_ptr<LdeManager> {
        using FieldElementT = typename decltype(field_tag)::type;

        {
          auto ptr = dynamic_cast<const MultiplicativeFftBases<
              FieldElementT, MultiplicativeGroupOrdering::kNaturalOrder>*>(&bases);
          if (ptr != nullptr) {
            return std::make_unique<LdeManagerTmpl<
                MultiplicativeLde<MultiplicativeGroupOrdering::kNaturalOrder, FieldElementT>>>(
                *ptr);
          }
        }
        auto ptr = dynamic_cast<const MultiplicativeFftBases<
            FieldElementT, MultiplicativeGroupOrdering::kBitReversedOrder>*>(&bases);
        ASSERT_RELEASE(ptr != nullptr, "The underlying type of FftBases is wrong");
        return std::make_unique<LdeManagerTmpl<
            MultiplicativeLde<MultiplicativeGroupOrdering::kBitReversedOrder, FieldElementT>>>(
            *ptr);
      },
      bases.GetField());
}

std::unique_ptr<LdeManager> MakeLdeManager(
    const OrderedGroup& source_domain_group, const FieldElement& source_eval_coset_offset) {
  return MakeLdeManagerImpl<MultiplicativeGroupOrdering::kNaturalOrder>(
      source_domain_group, source_eval_coset_offset);
}

std::unique_ptr<LdeManager> MakeBitReversedOrderLdeManager(
    const OrderedGroup& source_domain_group, const FieldElement& source_eval_coset_offset) {
  return MakeLdeManagerImpl<MultiplicativeGroupOrdering::kBitReversedOrder>(
      source_domain_group, source_eval_coset_offset);
}

}  // namespace starkware
