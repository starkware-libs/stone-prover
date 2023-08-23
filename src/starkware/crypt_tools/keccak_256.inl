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

#include "starkware/crypt_tools/keccak_256.h"

#include "starkware/crypt_tools/utils.h"
#include "starkware/error_handling/error_handling.h"
#include "starkware/stl_utils/containers.h"
#include "starkware/utils/to_from_string.h"

namespace starkware {

// On x86 we use an optimized implemenation based AVX2 extensions.
// The current convention in the code is to assume !__EMSCRIPTEN__ => x86.
#ifndef __EMSCRIPTEN__
#define USE_AVX2
#endif

#ifdef USE_AVX2
extern "C" void KeccakP1600_Permute_24rounds(std::byte* state);  // NOLINT
#else
extern "C" void KeccakF1600_StatePermute(std::byte* state);  // NOLINT
#endif

/*
  This is an internal class that represents the internal 1600bits state of Keccak256.
  It exposes functions needed to calculate the Keccak256 hash.

  We implement the xor data with state ourselves as it is relatively simply and it allows us
  to use different keccak impelmentations without worrying about the specifics of the API or padding
  that they are using.
*/
class alignas(32) Keccak256::keccak_state {
 public:
  static constexpr size_t kBlockBytes = (1600 - 512) / 8;

  static size_t AdjustedLaneOffset(uint64_t word_offset) {
#ifdef USE_AVX2
    // The AVX2 implementation stores the state in a permutated order.
    // The mapState translates from normal order to the premutated order.
    static constexpr std::array<size_t, 25> kMapState = {
        0 * 8,  1 * 8,  2 * 8,  3 * 8,  4 * 8,  7 * 8,  21 * 8, 10 * 8, 15 * 8,
        20 * 8, 5 * 8,  13 * 8, 22 * 8, 19 * 8, 12 * 8, 8 * 8,  9 * 8,  18 * 8,
        23 * 8, 16 * 8, 6 * 8,  17 * 8, 14 * 8, 11 * 8, 24 * 8};

    return UncheckedAt(gsl::make_span(kMapState), word_offset);
#else
    return word_offset * sizeof(uint64_t);
#endif
  }

  void WordAlignedXorWithState(gsl::span<const std::byte> bytes, size_t word_offset = 0) {
    size_t word_count = bytes.size() / sizeof(uint64_t);
    for (size_t i = 0; i < word_count; ++i) {
      XorLane(
          &UncheckedAt(gsl::make_span(st), AdjustedLaneOffset(word_offset + i)),
          &UncheckedAt(bytes, i * sizeof(uint64_t)));
    }
  }

  std::array<std::byte, kStateNumBytes> ExtractFullState() {
    std::array<std::byte, kStateNumBytes> res{};
    constexpr size_t kWordCount = kStateNumBytes / sizeof(uint64_t);
    for (size_t i = 0; i < kWordCount; ++i) {
      memcpy(
          &UncheckedAt(gsl::make_span(res), i * sizeof(uint64_t)),
          &UncheckedAt(gsl::make_span(st), AdjustedLaneOffset(i)), sizeof(uint64_t));
    }
    return res;
  }

  void XorLane(std::byte* lane, const std::byte* bytes) {
    uint64_t lane_data;
    uint64_t input_data;
    memcpy(&lane_data, lane, sizeof(uint64_t));
    memcpy(&input_data, bytes, sizeof(uint64_t));
    lane_data ^= input_data;
    memcpy(lane, &lane_data, sizeof(uint64_t));
  }

  void UnalignedXorWithState(gsl::span<const std::byte> bytes) {
    size_t unaligned = bytes.length() & (sizeof(uint64_t) - 1);
    size_t offset = bytes.length() - unaligned;
    WordAlignedXorWithState(bytes.first(offset));

    auto state_span = gsl::make_span(st);
    size_t adjusted_offset = AdjustedLaneOffset(offset / sizeof(uint64_t));
    for (size_t i = 0; i < unaligned; ++i) {
      UncheckedAt(state_span, adjusted_offset + i) ^= UncheckedAt(bytes, offset + i);
    }
  }

  static size_t AdjustedByteOffset(size_t byte_offset) {
    return AdjustedLaneOffset(byte_offset / sizeof(uint64_t)) |
           (byte_offset & (sizeof(uint64_t) - 1));
  }

  void Finalize(size_t padding_offset) {
    // Original Keccak padding (used by the implementation in Ethereum).
    // For sha3 replace with 0x6;
    st.at(AdjustedByteOffset(padding_offset)) ^= std::byte(0x1);
    st.at(AdjustedByteOffset(kBlockBytes - 1)) ^= std::byte(0x80);

    ApplyPermutation();
  }

  void ApplyPermutation() {
#ifdef USE_AVX2
    KeccakP1600_Permute_24rounds(st.data());
#else
    KeccakF1600_StatePermute(st.data());
#endif
  }

  Keccak256 ExtractState() {
    // There is no permutation of the first 256bits so we can copy them as is.
    return Keccak256::InitDigestTo(gsl::make_span(st).first(kDigestNumBytes));
  }

  std::array<std::byte, kStateNumBytes> st{};
};

#undef USE_AVX2

inline Keccak256::Keccak256(const gsl::span<const std::byte>& data)
    : buffer_(InitDigestFromSpan<Keccak256>(data)) {}

inline Keccak256 Keccak256::InitDigestTo(const gsl::span<const std::byte>& digest) {
  return Keccak256(digest);
}

inline Keccak256 Keccak256::HashBytesWithLength(gsl::span<const std::byte> bytes) {
  keccak_state state;

  size_t bytes_to_hash = bytes.size();
  size_t offset = 0;
  size_t bytes_in_block = keccak_state::kBlockBytes;

  // Absorb full block of bytes_in_block bytes.
  while (bytes_to_hash >= bytes_in_block) {
    state.WordAlignedXorWithState(bytes.subspan(offset, bytes_in_block));
    state.ApplyPermutation();
    offset += bytes_in_block;
    bytes_to_hash -= bytes_in_block;
  }

  // Absorb the last partial (or empty) block, add padding and return the result.
  state.UnalignedXorWithState(bytes.last(bytes_to_hash));
  state.Finalize(bytes_to_hash);
  return state.ExtractState();
}

inline std::array<std::byte, Keccak256::kStateNumBytes> Keccak256::ApplyPermutation(
    gsl::span<const std::byte> bytes) {
  keccak_state state;

  ASSERT_RELEASE(bytes.size() == kStateNumBytes, "Wrong input length.");

  state.WordAlignedXorWithState(bytes);
  state.ApplyPermutation();
  return state.ExtractFullState();
}

inline Keccak256 Keccak256::HashBytesWithLength(
    gsl::span<const std::byte> bytes, const Keccak256& initial_hash) {
  return Hash(initial_hash, HashBytesWithLength(bytes));
}

inline Keccak256 Keccak256::Hash(const Keccak256& val1, const Keccak256& val2) {
  keccak_state state;

  state.WordAlignedXorWithState(val1.buffer_, 0);
  state.WordAlignedXorWithState(val2.buffer_, val1.buffer_.size() / sizeof(uint64_t));
  state.Finalize(2 * kDigestNumBytes);
  return state.ExtractState();
}

inline std::string Keccak256::ToString() const { return BytesToHexString(buffer_); }

inline std::ostream& operator<<(std::ostream& out, const Keccak256& sha) {
  return out << sha.ToString();
}

inline bool Keccak256::operator==(const Keccak256& other) const { return buffer_ == other.buffer_; }
inline bool Keccak256::operator!=(const Keccak256& other) const { return buffer_ != other.buffer_; }

}  // namespace starkware
