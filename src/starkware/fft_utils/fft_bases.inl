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
#include "starkware/algebra/utils/invoke_template_version.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

template <typename FieldElementT>
FieldElementT ApplyDimOneVanishingPolynomial(
    const FieldElementT& point, const FieldElementT& basis_element) {
  return point * (point + basis_element);
}

template <typename BasesT, typename GroupT>
std::tuple<std::unique_ptr<FftBases>, std::vector<FieldElement>>
FftBasesCommonImpl<BasesT, GroupT>::SplitToCosets(size_t n_log_cosets) const {
  ASSERT_RELEASE(!bases_.empty(), "Can't split empty bases");
  DomainT domain = bases_[0];
  const std::vector<FieldElementT>& basis = domain.Basis();
  ASSERT_RELEASE(basis.size() >= n_log_cosets, "Too many cosets requested");

  auto [coset_bases, offsets_domain] = BasesT::SplitDomain(domain, n_log_cosets);  // NOLINT
  std::vector<FieldElement> offsets({offsets_domain.begin(), offsets_domain.end()});
  ASSERT_RELEASE(offsets.size() == Pow2(n_log_cosets), "Wrong number of offsets");

  return {std::make_unique<BasesT>(std::move(coset_bases)), std::move(offsets)};
}

template <typename FieldElementT, MultiplicativeGroupOrdering Order>
MultiplicativeFftBases<FieldElementT, Order>::MultiplicativeFftBases(DomainT domain) {
  std::vector<FieldElementT> basis = domain.Basis();
  bool reversed_order = !IsNaturalOrder(domain);

  FieldElementT current_offset = domain.StartOffset();

  for (size_t i = 0; i < basis.size(); ++i) {
    if (reversed_order) {
      this->bases_.emplace_back(
          std::vector<FieldElementT>{basis.begin(), basis.end() - i}, current_offset);
    } else {
      this->bases_.emplace_back(
          std::vector<FieldElementT>{basis.begin() + i, basis.end()}, current_offset);
    }
    current_offset = ApplyBasisTransformTmpl(current_offset, i);
  }
  this->bases_.emplace_back(std::vector<FieldElementT>{}, current_offset);
}

template <typename Func>
auto InvokeBasesTemplateVersion(const Func& func, const FftBases& bases) {
  return InvokeFieldTemplateVersion(
      [&](auto field_tag) {
        using FieldElementT = typename decltype(field_tag)::type;

        // MultiplicativeFftBases<..., kNaturalOrder>.
        auto ptr_multiplicative_natural = dynamic_cast<const MultiplicativeFftBases<
            FieldElementT, MultiplicativeGroupOrdering::kNaturalOrder>*>(&bases);
        if (ptr_multiplicative_natural != nullptr) {
          return func(*ptr_multiplicative_natural);
        }

        // MultiplicativeFftBases<..., kBitReversedOrder>.
        auto ptr_multiplicative_reversed = dynamic_cast<const MultiplicativeFftBases<
            FieldElementT, MultiplicativeGroupOrdering::kBitReversedOrder>*>(&bases);
        if (ptr_multiplicative_reversed != nullptr) {
          return func(*ptr_multiplicative_reversed);
        }

        ASSERT_RELEASE(false, "The underlying type of FftBases is wrong");
      },
      bases.GetField());
}

}  // namespace starkware
