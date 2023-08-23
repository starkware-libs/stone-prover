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

#ifndef STARKWARE_RANDOMNESS_HASH_CHAIN_H_
#define STARKWARE_RANDOMNESS_HASH_CHAIN_H_

#include <cstddef>
#include <limits>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

template <typename HashT>
class HashChain {
 public:
  /*
    Initializes the hash chain to a value based on the public input and the constraints system. This
    ensures that the initial randomness is dependent on the current instance, and not on
    prover-defined data.
  */
  explicit HashChain(gsl::span<const std::byte> public_input_data);
  HashChain();

  void InitHashChain(gsl::span<const std::byte> bytes);

  void GetRandomBytes(gsl::span<std::byte> random_bytes_out);

  /*
    Hash data of arbitrary length into the hash chain.
  */
  void UpdateHashChain(gsl::span<const std::byte> raw_bytes);

  /*
    Similar to UpdateHashChain but the seed is incremented by 'seed_increment' before mixing it with
    the hashchain. This is done to create domain seperation between MixSeedWithBytes and
    GetRandomBytes.
  */
  void MixSeedWithBytes(gsl::span<const std::byte> raw_bytes, uint64_t seed_increment);

  const HashT& GetHashChainState() const { return hash_; }

  // In order to allow this class to be used by 'std::uniform_int_distribution' as a random bit
  // generator it must support the C++ standard UniformRandomBitGenerator requirements (see
  // https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator) which requires the bit
  // generator to support the 'oprator()', 'min()', 'max()' and result_type.
  // NOTE: This API is only used for random integer generation in tests and should not be used for
  // production as it limits the number of random bits that can be generated to 64.
  using result_type = uint64_t;
  static constexpr result_type min() { return 0; }                                     // NOLINT
  static constexpr result_type max() { return std::numeric_limits<uint64_t>::max(); }  // NOLINT
  result_type operator()() {
    result_type return_val;
    GetRandomBytes(gsl::make_span<result_type>(&return_val, 1).template as_span<std::byte>());
    return return_val;
  }

 private:
  /*
    A standard way to generate any number of pseudo random bytes from a given digest and a given
    hash function is to hash the digest with an incrementing counter until sufficient bytes are
    generated.
    This function takes a counter and a hash and returns their combined hash.
  */
  static HashT HashWithCounter(const HashT& hash, uint64_t counter);

  /*
    Adds additional random bytes by hashing the value of the given counter together with the
    current hash chain. num_bytes must be equal or less than the hash digest size.
  */
  void GetMoreRandomBytesUsingHashWithCounter(
      uint64_t counter, gsl::span<std::byte> random_bytes_out);

  HashT hash_;
  std::array<std::byte, HashT::kDigestNumBytes * 2> spare_bytes_{};
  size_t num_spare_bytes_ = 0;
  size_t counter_ = 0;
};

}  // namespace starkware

#endif  // STARKWARE_RANDOMNESS_HASH_CHAIN_H_
