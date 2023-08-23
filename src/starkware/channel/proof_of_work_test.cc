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

#include "starkware/channel/proof_of_work.h"

#include "gtest/gtest.h"

#include "starkware/channel/noninteractive_prover_channel.h"
#include "starkware/channel/noninteractive_verifier_channel.h"
#include "starkware/crypt_tools/keccak_256.h"

namespace starkware {
namespace {

TEST(ProofOfWork, Completeness) {
  Prng prng;
  ProofOfWorkProver<Keccak256> pow_prover;

  const size_t work_bits = 15;
  auto witness = pow_prover.Prove(prng.GetPrngState(), work_bits);

  ProofOfWorkVerifier<Keccak256> pow_verifier;
  EXPECT_TRUE(pow_verifier.Verify(prng.GetPrngState(), work_bits, witness));
}

TEST(ProofOfWork, Soundness) {
  Prng prng;
  ProofOfWorkProver<Keccak256> pow_prover;

  const size_t work_bits = 15;
  auto witness = pow_prover.Prove(prng.GetPrngState(), work_bits);

  ProofOfWorkVerifier<Keccak256> pow_verifier;
  EXPECT_FALSE(pow_verifier.Verify(prng.GetPrngState(), work_bits + 1, witness));
  EXPECT_FALSE(pow_verifier.Verify(prng.GetPrngState(), work_bits - 1, witness));
}

TEST(ProofOfWork, BitChange) {
  Prng prng;
  ProofOfWorkProver<Keccak256> pow_prover;

  const size_t work_bits = 15;
  auto witness = pow_prover.Prove(prng.GetPrngState(), work_bits);

  ProofOfWorkVerifier<Keccak256> pow_verifier;

  for (std::byte& witness_byte : witness) {
    for (size_t bit_index = 0; bit_index < 8; ++bit_index) {
      witness_byte ^= std::byte(1 << bit_index);
      ASSERT_FALSE(pow_verifier.Verify(prng.GetPrngState(), work_bits, witness));
      witness_byte ^= std::byte(1 << bit_index);
    }
  }
}

#ifndef __EMSCRIPTEN__
TEST(ProofOfWork, ParallelCompleteness) {
  Prng prng;
  ProofOfWorkProver<Keccak256> pow_prover;
  auto task_manager = TaskManager::CreateInstanceForTesting(8);
  const size_t work_bits = 18;
  auto witness = pow_prover.Prove(prng.GetPrngState(), work_bits, &task_manager, 15);

  ProofOfWorkVerifier<Keccak256> pow_verifier;
  EXPECT_TRUE(pow_verifier.Verify(prng.GetPrngState(), work_bits, witness));
}
#endif

}  // namespace
}  // namespace starkware
