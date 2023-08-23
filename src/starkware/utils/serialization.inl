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

#include "starkware/utils/serialization.h"

#include <endian.h>
#include <algorithm>
#include <utility>

#include "starkware/algebra/big_int.h"
#include "starkware/error_handling/error_handling.h"

namespace starkware {

/*
  Place holder until we implement the logic to control the Endianness.
*/
inline bool UseBigEndianSerialization() { return true; }

/*
  C++17 doesn't support function template partial specialization.
  I.e. template <size_t N> Serialize<BigInt<N>(...) doesn't compile.

  We use a templated class as a workaround.
*/
template <typename T, bool BigEndianSerialization>
class SerializationImpl {
  // static_assert(false) does not depend on T, so we use sizeof(T) == 0 which is always false.
  static_assert(sizeof(T) == 0, "Serialization is not implemented for the given type.");
};

template <bool BigEndianSerialization>
class SerializationImpl<uint16_t, BigEndianSerialization> {
 public:
  static void Serialize(uint16_t val, const gsl::span<std::byte> span_out) {
    ASSERT_DEBUG(
        span_out.size() == sizeof(uint16_t), "Destination span size mismatches uint16_t size.");
    val = BigEndianSerialization ? htobe16(val) : htole16(val);   // NOLINT
    const auto bytes = gsl::byte_span(val).as_span<std::byte>();  // NOLINT
    std::copy(bytes.begin(), bytes.end(), span_out.begin());
  }

  static uint16_t Deserialize(const gsl::span<const std::byte> span) {
    ASSERT_DEBUG(span.size() == sizeof(uint16_t), "source span size mismatches uint16_t size.");
    uint16_t val;
    const auto bytes = gsl::byte_span(val).as_span<std::byte>();  // NOLINT
    // Use span.begin() + sizeof(uint16_t) rather than span.end() to allow the compiler to
    // deduce the size is a compile time const.
    std::copy(span.begin(), span.begin() + sizeof(uint16_t), bytes.begin());
    return BigEndianSerialization ? be16toh(val) : le16toh(val);  // NOLINT
  }
};

template <bool BigEndianSerialization>
class SerializationImpl<uint32_t, BigEndianSerialization> {
 public:
  static void Serialize(uint32_t val, const gsl::span<std::byte> span_out) {
    ASSERT_DEBUG(
        span_out.size() == sizeof(uint32_t), "Destination span size mismatches uint32_t size.");
    val = BigEndianSerialization ? htobe32(val) : htole32(val);   // NOLINT
    const auto bytes = gsl::byte_span(val).as_span<std::byte>();  // NOLINT
    std::copy(bytes.begin(), bytes.end(), span_out.begin());
  }

  static uint32_t Deserialize(const gsl::span<const std::byte> span) {
    ASSERT_DEBUG(span.size() == sizeof(uint32_t), "source span size mismatches uint32_t size.");
    uint32_t val;
    const auto bytes = gsl::byte_span(val).as_span<std::byte>();  // NOLINT
    // Use span.begin() + sizeof(uint32_t) rather than span.end() to allow the compiler to
    // deduce the size is a compile time const.
    std::copy(span.begin(), span.begin() + sizeof(uint32_t), bytes.begin());
    return BigEndianSerialization ? be32toh(val) : le32toh(val);  // NOLINT
  }
};

template <bool BigEndianSerialization>
class SerializationImpl<uint64_t, BigEndianSerialization> {
 public:
  static void Serialize(uint64_t val, const gsl::span<std::byte> span_out) {
    ASSERT_DEBUG(
        span_out.size() == sizeof(uint64_t), "Destination span size mismatches uint64_t size.");
    val = BigEndianSerialization ? htobe64(val) : htole64(val);   // NOLINT
    const auto bytes = gsl::byte_span(val).as_span<std::byte>();  // NOLINT
    std::copy(bytes.begin(), bytes.end(), span_out.begin());
  }

  static uint64_t Deserialize(const gsl::span<const std::byte> span) {
    ASSERT_DEBUG(span.size() == sizeof(uint64_t), "source span size mismatches uint64_t size.");
    uint64_t val;
    const auto bytes = gsl::byte_span(val).as_span<std::byte>();  // NOLINT
    // Use span.begin() + sizeof(uint64_t) rather than span.end() to allow the compiler to
    // deduce the size is a compile time const.
    std::copy(span.begin(), span.begin() + sizeof(uint64_t), bytes.begin());
    return BigEndianSerialization ? be64toh(val) : le64toh(val);  // NOLINT
  }
};

template <size_t N, bool BigEndianSerialization>
class SerializationImpl<BigInt<N>, BigEndianSerialization> {
 public:
  static void Serialize(const BigInt<N>& val, const gsl::span<std::byte> span_out) {
    ASSERT_DEBUG(span_out.size() == BigInt<N>::SizeInBytes(), "Span size mismatches BigInt size");
    for (size_t i = 0; i < N; ++i) {
      size_t index = BigEndianSerialization ? N - i - 1 : i;
      SerializationImpl<uint64_t, BigEndianSerialization>::Serialize(
          val[index], span_out.subspan(i * sizeof(uint64_t), sizeof(uint64_t)));
    }
  }

  static BigInt<N> Deserialize(const gsl::span<const std::byte> span) {
    ASSERT_DEBUG(
        span.size() == BigInt<N>::SizeInBytes(), "Source span size mismatches BigInt size");
    std::array<uint64_t, N> value{};
    for (size_t i = 0; i < N; ++i) {
      size_t index = BigEndianSerialization ? N - i - 1 : i;
      value.at(index) = SerializationImpl<uint64_t, BigEndianSerialization>::Deserialize(
          span.subspan(i * sizeof(uint64_t), sizeof(uint64_t)));
    }
    return BigInt(value);
  }
};

template <typename T>
void Serialize(const T& val, gsl::span<std::byte> span_out) {
  Serialize<T>(val, span_out, UseBigEndianSerialization());
}

template <typename T>
T Deserialize(gsl::span<const std::byte> span) {
  return Deserialize<T>(span, UseBigEndianSerialization());
}

template <typename T>
void Serialize(const T& val, const gsl::span<std::byte> span_out, bool use_big_endian) {  // NOLINT
  // Nolint here is due to a bug with declaration/definition of function with/without const
  // qualifiers.
  if (use_big_endian) {
    SerializationImpl<T, true>::Serialize(val, span_out);
  } else {
    SerializationImpl<T, false>::Serialize(val, span_out);
  }
}

template <typename T>
T Deserialize(const gsl::span<const std::byte> span, bool use_big_endian) {
  if (use_big_endian) {
    return SerializationImpl<T, true>::Deserialize(span);
  }
  return SerializationImpl<T, false>::Deserialize(span);
}

template <size_t N>
BigInt<N> EncodeStringAsBigInt(const std::string& str) {
  ASSERT_RELEASE(
      str.length() <= BigInt<N>::SizeInBytes(), "String length must be at most " +
                                                    std::to_string(BigInt<N>::SizeInBytes()) +
                                                    " ('" + str + "').");
  std::vector<std::byte> bytes;
  bytes.reserve(BigInt<N>::SizeInBytes());
  for (size_t i = 0; i < BigInt<N>::SizeInBytes() - str.length(); ++i) {
    bytes.push_back(std::byte(0));
  }
  for (const auto& chr : str) {
    bytes.push_back(std::byte(chr));
  }
  return Deserialize<BigInt<N>>(bytes, /*use_big_endian=*/true);
}

}  // namespace starkware
