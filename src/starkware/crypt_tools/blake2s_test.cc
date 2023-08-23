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

#include "starkware/crypt_tools/blake2s.h"

#include <cstddef>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/randomness/prng.h"
#include "starkware/stl_utils/containers.h"

namespace starkware {

const std::array<std::byte, 2 * Blake2s256::kDigestNumBytes> kHelloWorld = MakeByteArray<
    'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!', 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60>();
const std::array<std::byte, 12> kHelloWorldSmall =
    MakeByteArray<'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'>();
// This was obtained using python3's hashlib.blake2s.
const std::array<std::byte, Blake2s256::kDigestNumBytes> kResultFull = MakeByteArray<
    0xbe, 0x8c, 0x67, 0x77, 0xe8, 0x8d, 0x28, 0x7d, 0xd9, 0x27, 0x97, 0x53, 0x27, 0xdd, 0x42, 0x14,
    0xd1, 0x99, 0xa1, 0xa1, 0xb6, 0x7f, 0xe2, 0xe2, 0x66, 0x66, 0xcc, 0x33, 0x65, 0x33, 0x66,
    0x6a>();
// This was ran once.
const std::array<std::byte, Blake2s256::kDigestNumBytes> kResultPartial = MakeByteArray<
    0x93, 0x67, 0x40, 0x36, 0xA1, 0xF3, 0x30, 0x39, 0x40, 0x97, 0x57, 0xA7, 0x01, 0x79, 0x75, 0x2E,
    0x79, 0x68, 0x5E, 0x16, 0xCA, 0x48, 0x1E, 0x6B, 0xAC, 0x83, 0x01, 0xBB, 0x11, 0x8D, 0x8B,
    0xED>();

namespace {

TEST(Blake2s256, HelloWorldHash) {
  gsl::span<const std::byte> hello_world_span(kHelloWorld);
  gsl::span<const std::byte> hello_world1 =
      hello_world_span.subspan(0, Blake2s256::kDigestNumBytes);
  gsl::span<const std::byte> hello_world2 = hello_world_span.subspan(Blake2s256::kDigestNumBytes);
  Blake2s256 hashed_hello_world = Blake2s256::HashWithoutFinalize(
      Blake2s256::InitDigestTo(hello_world1), Blake2s256::InitDigestTo(hello_world2));
  EXPECT_EQ(Blake2s256::InitDigestTo(kResultPartial), hashed_hello_world);
}

TEST(Blake2s256, HelloWorldHashFull) {
  gsl::span<const std::byte> hello_world_span(kHelloWorldSmall);
  Blake2s256 hashed_hello_world = Blake2s256::HashBytesWithLength(hello_world_span);
  EXPECT_EQ(Blake2s256::InitDigestTo(kResultFull), hashed_hello_world);
}

TEST(Blake2s256, HashTwoHashesWithLength) {
  // The following was computed using Python's hashlib by running:
  // h1 = blake2s("Hello World!".encode())
  // h2 = blake2s(h1.digest())
  // h3 = blake2s(h1.digest() + h2.digest()).
  const std::array<std::byte, Blake2s256::kDigestNumBytes> k_result3 = MakeByteArray<
      0x2E, 0x51, 0xDD, 0x07, 0x53, 0xF7, 0x55, 0x2D, 0xD3, 0x0D, 0xC5, 0xA0, 0x49, 0xB9, 0x6F,
      0x24, 0xFE, 0xDE, 0x8F, 0x36, 0x3F, 0x19, 0xA8, 0x73, 0x86, 0x05, 0x6C, 0x40, 0x94, 0x40,
      0x6B, 0x68>();
  Blake2s256 h1 = Blake2s256::HashBytesWithLength(kHelloWorldSmall);
  Blake2s256 h2 = Blake2s256::HashBytesWithLength(h1.GetDigest());
  Blake2s256 h3 = Blake2s256::Hash(h1, h2);
  EXPECT_EQ(Blake2s256::InitDigestTo(k_result3), h3);
}

TEST(Blake2s256, OutStreamOperator) {
  gsl::span<const std::byte> hello_world_span(kHelloWorldSmall);
  Blake2s256 hashed_hello_world = Blake2s256::HashBytesWithLength(hello_world_span);
  std::stringstream ss;
  ss << hashed_hello_world;
  EXPECT_EQ(hashed_hello_world.ToString(), ss.str());
  EXPECT_EQ("0xbe8c6777e88d287dd927975327dd4214d199a1a1b67fe2e26666cc336533666a", ss.str());
}

}  // namespace

}  // namespace starkware
