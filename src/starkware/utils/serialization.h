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

#ifndef STARKWARE_UTILS_SERIALIZATION_H_
#define STARKWARE_UTILS_SERIALIZATION_H_

#include <cstddef>
#include <string>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

template <typename T>
void Serialize(const T& val, gsl::span<std::byte> span_out);

template <typename T>
T Deserialize(gsl::span<const std::byte> span);

template <typename T>
void Serialize(const T& val, gsl::span<std::byte> span_out, bool use_big_endian);

template <typename T>
T Deserialize(gsl::span<const std::byte> span, bool use_big_endian);

// Forward declaration of BigInt<N>.
template <size_t N>
class BigInt;

/*
  Encodes a short (up to 8*N characters) string as BigInt<N>.
*/
template <size_t N>
BigInt<N> EncodeStringAsBigInt(const std::string& str);

}  // namespace starkware

#include "starkware/utils/serialization.inl"

#endif  // STARKWARE_UTILS_SERIALIZATION_H_
