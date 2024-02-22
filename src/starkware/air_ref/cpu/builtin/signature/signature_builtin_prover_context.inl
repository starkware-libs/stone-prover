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

#include "starkware/utils/task_manager.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
auto SignatureBuiltinProverContext<FieldElementT>::GetDummySignature() const -> SigInputT {
  Prng prng(std::array<std::byte, 0>{});
  const typename FieldElementT::ValueType dummy_private_key = FieldElementT::ValueType::One();
  const EcPoint<FieldElementT> public_key = this->sig_config_.generator_point.MultiplyByScalar(
      dummy_private_key, this->sig_config_.alpha);
  static constexpr typename FieldElementT::ValueType kDummyMessage =
      FieldElementT::ValueType::One();

  const auto& [r, w] = EcdsaComponent<FieldElementT>::Sign(
      this->sig_config_, dummy_private_key, kDummyMessage, &prng);
  return SigInputT{public_key, FieldElementT::ConstexprFromBigInt(kDummyMessage), r, w};
}

template <typename FieldElementT>
void SignatureBuiltinProverContext<FieldElementT>::WriteTrace(
    gsl::span<const gsl::span<FieldElementT>> trace) const {
  const SigInputT dummy_input(GetDummySignature());

  for (uint64_t idx = 0; idx < n_instances_; ++idx) {
    const uint64_t mem_addr = begin_addr_ + 2 * idx;

    const auto& input_itr = inputs_.find(idx);
    const SigInputT& input = input_itr != inputs_.end() ? input_itr->second : dummy_input;
    signature_components_[idx % repetitions_]->WriteTrace(input, idx / repetitions_, trace);

    mem_pubkey_.WriteTrace(idx, mem_addr, input.public_key.x, trace);
    mem_message_.WriteTrace(idx, mem_addr + 1, input.z, trace);
  }
}

template <typename FieldElementT>
auto SignatureBuiltinProverContext<FieldElementT>::ParsePrivateInput(
    const JsonValue& private_input, const SigConfigT& sig_config) -> std::map<uint64_t, SigInputT> {
  std::map<uint64_t, SigInputT> res;

  for (size_t i = 0; i < private_input.ArrayLength(); ++i) {
    const auto& input = private_input[i];
    res.emplace(
        input["index"].AsUint64(),
        SigInputT::FromPartialPublicKey(
            /*public_key_x=*/input["pubkey"].AsFieldElement<FieldElementT>(),
            /*z=*/input["msg"].AsFieldElement<FieldElementT>(),
            /*r=*/input["signature_input"]["r"].AsFieldElement<FieldElementT>(),
            /*w=*/input["signature_input"]["w"].AsFieldElement<FieldElementT>(),
            /*config=*/sig_config));
  }

  return res;
}

}  // namespace cpu
}  // namespace starkware
