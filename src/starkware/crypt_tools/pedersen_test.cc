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

#include "starkware/crypt_tools/pedersen.h"

#include <cstddef>
#include <string>
#include <vector>

#include "glog/logging.h"
#include "gtest/gtest.h"
#include "third_party/gsl/gsl-lite.hpp"

#include "starkware/algebra/big_int.h"
#include "starkware/crypt_tools/test_utils.h"
#include "starkware/utils/serialization.h"

namespace starkware {
namespace {

Pedersen AsDigest(const BigInt<4>& val) {
  std::array<std::byte, BigInt<4>::SizeInBytes()> bytes{};
  Serialize(val, bytes, /*use_big_endian=*/true);

  return Pedersen::InitDigestTo(bytes);
}

TEST(Pedersen, EmptyString) {
  std::array<std::byte, 0> in{};

  EXPECT_EQ(
      AsDigest(0x49ee3eba8c1600700ee1b87eb599f16716b0b1022947733551fde4050ca6804_Z),
      Pedersen::HashBytesWithLength(in));
}

TEST(Pedersen, TestVectors) {
  EXPECT_EQ(
      AsDigest(0x76ee717494854a0656535e7ebd851daf6daced15363a94e3c14587187818208_Z),
      Pedersen::HashBytesWithLength(GenerateTestVector(32)));

  EXPECT_EQ(
      AsDigest(0x5ae166bb6f7bd5aecc639a736a38257f0913611c4967e6fb1cabd4278790e4c_Z),
      Pedersen::HashBytesWithLength(GenerateTestVector(64)));
}

}  // namespace
}  // namespace starkware
