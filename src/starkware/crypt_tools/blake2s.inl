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

#include "starkware/crypt_tools/utils.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/utils/to_from_string.h"

#ifndef __EMSCRIPTEN__
#include "third_party/blake2/blake2-impl.h"
#include "third_party/blake2/blake2.h"
#else
#include "third_party/blake2_ref/blake2-impl.h"
#include "third_party/blake2_ref/blake2.h"
#endif

namespace starkware {

template <size_t DigestNumBits>
Blake2s<DigestNumBits>::Blake2s(const gsl::span<const std::byte>& data)
    : buffer_(InitDigestFromSpan<Blake2s<DigestNumBits>>(data)) {}

template <size_t DigestNumBits>
const Blake2s<DigestNumBits> Blake2s<DigestNumBits>::InitDigestTo(
    const gsl::span<const std::byte>& digest) {
  return Blake2s<DigestNumBits>(digest);
}

template <size_t DigestNumBits>
const Blake2s<DigestNumBits> Blake2s<DigestNumBits>::HashWithoutFinalize(
    const Blake2s<DigestNumBits>& val1, const Blake2s<DigestNumBits>& val2) {
  std::array<uint8_t, BLAKE2S_BLOCKBYTES> data{};
  memcpy(data.data(), &val1, kDigestNumBytes);
  memcpy(&data[kDigestNumBytes], &val2, kDigestNumBytes);
  if constexpr (2 * kDigestNumBytes < BLAKE2S_BLOCKBYTES) {  // NOLINT: clang tidy if constexpr bug.
    memset(&data[2 * kDigestNumBytes], 0, BLAKE2S_BLOCKBYTES - 2 * kDigestNumBytes);
  }
  blake2s_state ctx;
  blake2s_init(&ctx, kDigestNumBytes);
  blake2s_compress(&ctx, data.data());

  // Semi finalize: only reverse endianess if needed for portability.
  Blake2s<DigestNumBits> result;
  for (size_t i = 0; i < kDigestNumDWords; ++i) { /* Output full hash to temp buffer */
    store32(
        gsl::make_span(result.buffer_)
            .template as_span<uint8_t>()
            .subspan(i * sizeof(uint32_t), sizeof(uint32_t))
            .data(),
        gsl::make_span(ctx.h).at(i));
  }

  return result;
}

template <size_t DigestNumBits>
const Blake2s<DigestNumBits> Blake2s<DigestNumBits>::Hash(
    const Blake2s<DigestNumBits>& val1, const Blake2s<DigestNumBits>& val2) {
  std::array<std::byte, 2 * kDigestNumBytes> data{};
  std::copy(val1.buffer_.begin(), val1.buffer_.end(), data.begin());
  std::copy(val2.buffer_.begin(), val2.buffer_.end(), data.begin() + kDigestNumBytes);
  return HashBytesWithLength(data);
}

template <size_t DigestNumBits>
const Blake2s<DigestNumBits> Blake2s<DigestNumBits>::HashBytesWithLength(
    gsl::span<const std::byte> bytes) {
  Blake2s<DigestNumBits> result;
  blake2s_state ctx;
  blake2s_init(&ctx, kDigestNumBytes);
  blake2s_update(&ctx, bytes.data(), bytes.size());
  blake2s_final(
      &ctx, gsl::make_span(result.buffer_).template as_span<uint8_t>().data(), kDigestNumBytes);

  return result;
}

template <size_t DigestNumBits>
const Blake2s<DigestNumBits> Blake2s<DigestNumBits>::HashBytesWithLength(
    gsl::span<const std::byte> bytes, const Blake2s<DigestNumBits>& initial_hash) {
  Blake2s<DigestNumBits> result;
  blake2s_state ctx;
  blake2s_init(&ctx, kDigestNumBytes);
  for (size_t i = 0; i < kDigestNumDWords; i++) {
    gsl::make_span(ctx.h).at(i) = load32(gsl::make_span(initial_hash.buffer_)
                                             .template as_span<const uint8_t>()
                                             .subspan(i * sizeof(uint32_t), sizeof(uint32_t))
                                             .data());
  }
  blake2s_update(&ctx, bytes.data(), bytes.size());
  blake2s_final(
      &ctx, gsl::make_span(result.buffer_).template as_span<uint8_t>().data(), kDigestNumBytes);

  return result;
}

template <size_t DigestNumBits>
bool Blake2s<DigestNumBits>::operator==(const Blake2s<DigestNumBits>& other) const {
  return buffer_ == other.buffer_;
}

template <size_t DigestNumBits>
bool Blake2s<DigestNumBits>::operator!=(const Blake2s<DigestNumBits>& other) const {
  return buffer_ != other.buffer_;
}

template <size_t DigestNumBits>
std::string Blake2s<DigestNumBits>::ToString() const {
  return BytesToHexString(buffer_);
}

inline std::ostream& operator<<(std::ostream& out, const Blake2s160& hash) {
  return out << hash.ToString();
}

inline std::ostream& operator<<(std::ostream& out, const Blake2s256& hash) {
  return out << hash.ToString();
}

}  // namespace starkware
