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

#ifndef STARKWARE_AIR_CPU_BUILTIN_SIGNATURE_SIGNATURE_BUILTIN_PROVER_CONTEXT_H_
#define STARKWARE_AIR_CPU_BUILTIN_SIGNATURE_SIGNATURE_BUILTIN_PROVER_CONTEXT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "starkware/air/components/ecdsa/ecdsa.h"
#include "starkware/air/components/memory/memory.h"

namespace starkware {
namespace cpu {

template <typename FieldElementT>
class SignatureBuiltinProverContext {
 public:
  using SigInputT = typename EcdsaComponent<FieldElementT>::Input;
  using SigConfigT = typename EcdsaComponent<FieldElementT>::Config;

  SignatureBuiltinProverContext(
      const std::string& name, const TraceGenerationContext& ctx,
      MemoryCell<FieldElementT>* memory_pool, const uint64_t begin_addr, uint64_t height,
      uint64_t n_hash_bits, uint64_t n_instances, size_t repetitions, const SigConfigT& sig_config,
      std::map<uint64_t, SigInputT> inputs)
      : begin_addr_(begin_addr),
        n_instances_(n_instances),
        repetitions_(repetitions),
        inputs_(std::move(inputs)),
        sig_config_(sig_config),
        mem_pubkey_(memory_pool, name + "/pubkey", ctx),
        mem_message_(memory_pool, name + "/message", ctx) {
    for (size_t rep = 0; rep < repetitions; rep++) {
      signature_components_.push_back(std::make_unique<EcdsaComponent<FieldElementT>>(
          name + "/signature" + std::to_string(rep), ctx, height, n_hash_bits, sig_config));
    }
  }

  SigInputT GetDummySignature() const;

  /*
    Writes the trace cells for the hash builtin.
  */
  void WriteTrace(gsl::span<const gsl::span<FieldElementT>> trace) const;

  /*
    Parses the private input for the signature builtin. private_input should be a list of
    objects of the form {
        "index": <index of instance>,
        "pubkey": <x coordinate of public key>,
        "msg": <message to sign>,
        "r": <signature's `r`>,
        "w": <signature's `w`>
    }.
  */
  static std::map<uint64_t, SigInputT> ParsePrivateInput(
      const JsonValue& private_input, const SigConfigT& sig_config);

 private:
  const uint64_t begin_addr_;
  const uint64_t n_instances_;
  const size_t repetitions_;
  const std::map<uint64_t, SigInputT> inputs_;
  std::vector<std::unique_ptr<const EcdsaComponent<FieldElementT>>> signature_components_;

  const SigConfigT sig_config_;
  const MemoryCellView<FieldElementT> mem_pubkey_;
  const MemoryCellView<FieldElementT> mem_message_;
};

}  // namespace cpu
}  // namespace starkware

#include "starkware/air/cpu/builtin/signature/signature_builtin_prover_context.inl"

#endif  // STARKWARE_AIR_CPU_BUILTIN_SIGNATURE_SIGNATURE_BUILTIN_PROVER_CONTEXT_H_
