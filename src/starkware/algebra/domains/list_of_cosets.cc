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

#include "starkware/algebra/domains/list_of_cosets.h"

#include <utility>

#include "starkware/algebra/field_operations.h"
#include "starkware/algebra/utils/invoke_template_version.h"

namespace starkware {

namespace {

template <typename FieldElementT>
std::vector<FieldElement> GetCosetsOffsets(
    const size_t n_cosets, const FieldElementT& domain_generator,
    const FieldElementT& common_offset) {
  // Define result vector.
  std::vector<FieldElement> result;
  result.reserve(n_cosets);

  // Compute the offsets vector.
  FieldElementT offset = common_offset;
  result.emplace_back(offset);
  for (size_t i = 1; i < n_cosets; i++) {
    offset *= domain_generator;
    result.emplace_back(offset);
  }

  return result;
}

}  // namespace

ListOfCosets::ListOfCosets(
    std::unique_ptr<FftBases>&& fft_bases, std::vector<FieldElement> cosets_offsets,
    FieldElement trace_generator, Field field)
    : fft_bases_(std::move(fft_bases)),
      cosets_offsets_(std::move(cosets_offsets)),
      trace_generator_(std::move(trace_generator)),
      field_(std::move(field)) {}

ListOfCosets ListOfCosets::MakeListOfCosets(
    size_t coset_size, size_t n_cosets, const Field& field, MultiplicativeGroupOrdering order) {
  // Compute generator of group containing all cosets.
  ASSERT_RELEASE(n_cosets > 0, "Number of cosets must be positive.");
  ASSERT_RELEASE(coset_size > 1, "Coset size must be > 1.");
  const size_t log_size = SafeLog2(coset_size);

  auto build_eval_domain = [&](auto field_tag) {
    using FieldElementT = typename decltype(field_tag)::type;

    FieldElementT coset_generator = FieldElementT::Uninitialized();
    FieldElementT trace_generator = FieldElementT::Uninitialized();
    FieldElementT offset = FieldElementT::Uninitialized();

    // Multiplicative case.
    uint64_t power_of_two_cosets = Pow2(Log2Ceil(n_cosets));

    coset_generator = GetSubGroupGenerator<FieldElementT>(coset_size * power_of_two_cosets);
    trace_generator = Pow(coset_generator, power_of_two_cosets);
    offset = FieldElementT::One();

    if (order == MultiplicativeGroupOrdering::kNaturalOrder) {
      MultiplicativeFftBases<FieldElementT, MultiplicativeGroupOrdering::kNaturalOrder> bases(
          trace_generator, log_size, offset);
      return ListOfCosets(
          std::make_unique<decltype(bases)>(std::move(bases)),
          GetCosetsOffsets(n_cosets, coset_generator, FieldElementT::GetBaseGenerator()),
          FieldElement(trace_generator), field);
    }
    MultiplicativeFftBases<FieldElementT, MultiplicativeGroupOrdering::kBitReversedOrder> bases(
        trace_generator, log_size, offset);
    return ListOfCosets(
        std::make_unique<decltype(bases)>(std::move(bases)),
        GetCosetsOffsets(n_cosets, coset_generator, FieldElementT::GetBaseGenerator()),
        FieldElement(trace_generator), field);
  };
  return InvokeFieldTemplateVersion(build_eval_domain, field);
}

FieldElement ListOfCosets::ElementByIndex(size_t coset_index, size_t group_index) const {
  ASSERT_RELEASE(coset_index < cosets_offsets_.size(), "Coset index out of range.");

  FieldElement point = Bases().At(0).GetFieldElementAt(group_index);
  FieldElement offset = cosets_offsets_[coset_index];

  return point * offset;
}

}  // namespace starkware
