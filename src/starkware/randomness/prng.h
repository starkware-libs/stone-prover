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

#ifndef STARKWARE_RANDOMNESS_PRNG_H_
#define STARKWARE_RANDOMNESS_PRNG_H_

#include <algorithm>
#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/crypt_tools/keccak_256.h"
#include "starkware/randomness/hash_chain.h"

namespace starkware {

/*
  This class is not thread safe.

  If one insisted on making it so - the inner random number generator can be made thread-local.
*/
class PrngBase {
 public:
  virtual ~PrngBase() = default;

  /*
    Clones the prng and wraps it in a unique_ptr.
  */
  virtual std::unique_ptr<PrngBase> Clone() const = 0;

  /*
    Resets the seed of the prng.
  */
  virtual void Reseed(gsl::span<const std::byte> bytes) = 0;

  /*
    Gets random bytes from the prng.
  */
  virtual void GetRandomBytes(gsl::span<std::byte> random_bytes_out) = 0;

  /*
    Calculates a new "random" seed based on the current seed and the given bytes.
  */
  virtual void MixSeedWithBytes(gsl::span<const std::byte> raw_bytes) = 0;

  /*
    Returns the current state of the prng as bytes.
  */
  virtual std::vector<std::byte> GetPrngState() const = 0;

  /*
    Return the name of the hash used by the prng.
  */
  virtual std::string GetHashName() const = 0;

  std::vector<std::byte> RandomByteVector(size_t n_elements) {
    std::vector<std::byte> return_vec(n_elements);
    GetRandomBytes(return_vec);
    return return_vec;
  }
};

template <typename HashT>
class PrngImpl : public PrngBase {
 public:
  /*
    Initializes seed using system time.
  */
  PrngImpl();
  ~PrngImpl() override = default;

  PrngImpl& operator=(const PrngImpl&) noexcept = delete;

  PrngImpl(PrngImpl&& src) noexcept = default;
  PrngImpl& operator=(PrngImpl&& other) noexcept {
    hash_chain_ = other.hash_chain_;
    return *this;
  }

  static std::unique_ptr<PrngBase> New() { return std::make_unique<PrngImpl>(); }

  std::unique_ptr<PrngBase> Clone() const override {
    // Copy manually since the copy constructor is private.
    PrngImpl copy(*this);
    return std::make_unique<PrngImpl>(std::move(copy));
  }

  explicit PrngImpl(gsl::span<const std::byte> bytes) : hash_chain_(HashChain<HashT>(bytes)) {}

  /*
    Expects a string with same format used to print the seed when the PRNG is initialized from
    system-time (using the default constructor). More specifically, the format is a string of
    hexadecimal digits in little endian, with a leading "0x" prefix. For example, if the string is
    "0x3F2BAA10", the returned PrngImpl is initialized using the bytes [10, AA, 2B, 3F].
  */
  static PrngImpl FromPrintout(const std::string& printout);

  void Reseed(gsl::span<const std::byte> bytes) override { hash_chain_.InitHashChain(bytes); }

  /*
    Returns a random number in the closed interval [min, max].
  */
  template <typename T>
  T UniformInt(T min, T max) {
    static_assert(std::is_integral<T>::value, "Type is not integral");
    ASSERT_RELEASE(min <= max, "Invalid interval");
    std::uniform_int_distribution<T> d(min, max);
    return d(hash_chain_);
  }

  template <typename T>
  std::vector<T> UniformIntVector(T min, T max, size_t n_elements) {
    static_assert(std::is_integral<T>::value, "Type is not integral");
    ASSERT_RELEASE(min <= max, "Invalid interval");
    std::uniform_int_distribution<T> d(min, max);
    std::vector<T> return_vec;
    return_vec.reserve(n_elements);
    for (size_t i = 0; i < n_elements; ++i) {
      return_vec.push_back(d(hash_chain_));
    }
    return return_vec;
  }

  template <typename T>
  T UniformBigInt(T min, T max) {
    T random_value = T::Zero();
    ASSERT_RELEASE(min <= max, "Invalid range");

    // This also works for max range.
    T range = max - min;
    T mask = T::One();
    if (range != T::Zero()) {
      mask <<= range.Log2Floor() + 1;
      mask = mask - T::One();
    }

    do {
      random_value = T::RandomBigInt(this) & mask;
    } while (random_value > range);  // Required to enforce uniformity.
    return min + random_value;
  }

  /*
    Returns a vector of DISTINCT random elements in the closed interval [min, max].
    The size of the vector is n_elements and in the elements are in the range between min
    and max.
  */
  template <typename T>
  std::vector<T> UniformDistinctIntVector(T min, T max, size_t n_elements) {
    static_assert(std::is_integral<T>::value, "Type is not integral");
    ASSERT_RELEASE(min <= max, "Invalid interval");
    ASSERT_RELEASE(
        static_cast<uint64_t>(n_elements) <= static_cast<uint64_t>((max - min) / 2),
        "Number of elements must be less than or equal to half the number of elements "
        "in the interval");
    std::uniform_int_distribution<T> d(min, max);
    std::vector<T> return_vec;
    return_vec.reserve(n_elements);
    std::unordered_set<T> current_set;
    while (current_set.size() < n_elements) {
      auto ret = current_set.insert(d(hash_chain_));
      if (ret.second) {
        return_vec.push_back(*(ret.first));
      }
    }
    return return_vec;
  }

  std::vector<bool> UniformBoolVector(size_t n_elements);

  template <typename T>
  std::vector<T> RandomFieldElementVector(size_t n_elements) {
    std::vector<T> return_vec;
    return_vec.reserve(n_elements);
    for (size_t i = 0; i < n_elements; ++i) {
      return_vec.push_back(T::RandomElement(this));
    }
    return return_vec;
  }

  void GetRandomBytes(gsl::span<std::byte> random_bytes_out) override {
    hash_chain_.GetRandomBytes(random_bytes_out);
  }

  template <typename OtherHashT>
  OtherHashT RandomHash() {
    return OtherHashT::InitDigestTo(RandomByteVector(OtherHashT::kDigestNumBytes));
  }

  void MixSeedWithBytes(gsl::span<const std::byte> raw_bytes) override {
    const uint64_t seed_increment = 1;

    hash_chain_.MixSeedWithBytes(raw_bytes, seed_increment);
  }

  std::vector<std::byte> GetPrngState() const override {
    auto digest = hash_chain_.GetHashChainState().GetDigest();
    return std::vector<std::byte>(digest.begin(), digest.end());
  }

  std::string GetHashName() const override { return HashT::HashName(); };

 private:
  // The HashChain class is used here for historical reasons.
  // The actual MixSeedWithBytes deviates slightly from a normal HashChain implementation.
  // For more detail, see The comment in HashChain<HashT>::MixSeedWithBytes.
  HashChain<HashT> hash_chain_;

  // Private copy construct to prevent copy by mistake, resulting in correlated randomness.
  PrngImpl(const PrngImpl&) = default;
};

using Prng = PrngImpl<Keccak256>;

}  // namespace starkware

#endif  // STARKWARE_RANDOMNESS_PRNG_H_
