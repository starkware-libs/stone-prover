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

#include <cstddef>
#include <limits>

#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/algebra/big_int.h"
#include "starkware/error_handling/test_utils.h"
#include "starkware/randomness/prng.h"

namespace starkware {
namespace {

using testing::ElementsAreArray;
using testing::HasSubstr;

template <typename T>
class SerializationTest : public ::testing::Test {
 public:
  Prng prng;
  // A convenience function to avoid the mess of "this->f.RandomElement(&this->prng)" every time.
  T RandomUint() {
    return prng.UniformInt<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
  }
};

using TestedIntTypes = ::testing::Types<uint32_t, uint64_t>;
TYPED_TEST_CASE(SerializationTest, TestedIntTypes);

// --- Test operators ---

TYPED_TEST(SerializationTest, SerializeDeserialize) {
  for (unsigned i = 0; i < 100; ++i) {
    auto a = this->RandomUint();
    std::array<std::byte, sizeof(TypeParam)> buf{};
    Serialize(a, buf);
    auto b = Deserialize<TypeParam>(buf);
    EXPECT_EQ(a, b);
  }
}

TYPED_TEST(SerializationTest, BigEndianness) {
  std::array<std::byte, sizeof(BigInt<4>)> arr{};
  Serialize(
      0x37ffd4ab5e008810ffffffffff6f800000000001330ffffffffffd737e000401_Z, arr,
      /*use_big_endian=*/true);
  std::array<uint8_t, sizeof(BigInt<4>)> expected{0x37, 0xff, 0xd4, 0xab, 0x5e, 0x00, 0x88, 0x10,
                                                  0xff, 0xff, 0xff, 0xff, 0xff, 0x6f, 0x80, 0x00,
                                                  0x00, 0x00, 0x00, 0x01, 0x33, 0x0f, 0xff, 0xff,
                                                  0xff, 0xff, 0xfd, 0x73, 0x7e, 0x00, 0x04, 0x01};

  EXPECT_THAT(gsl::make_span(expected).as_span<std::byte>(), ElementsAreArray(arr));
}

TYPED_TEST(SerializationTest, LittleEndianness) {
  std::array<std::byte, sizeof(BigInt<4>)> arr{};
  Serialize(
      0x37ffd4ab5e008810ffffffffff6f800000000001330ffffffffffd737e000401_Z, arr,
      /*use_big_endian=*/false);
  std::array<uint8_t, sizeof(BigInt<4>)> expected{0x01, 0x04, 0x00, 0x7e, 0x73, 0xfd, 0xff, 0xff,
                                                  0xff, 0xff, 0x0f, 0x33, 0x01, 0x00, 0x00, 0x00,
                                                  0x00, 0x80, 0x6f, 0xff, 0xff, 0xff, 0xff, 0xff,
                                                  0x10, 0x88, 0x00, 0x5e, 0xab, 0xd4, 0xff, 0x37};

  EXPECT_THAT(gsl::make_span(expected).as_span<std::byte>(), ElementsAreArray(arr));
}

TEST(EncodeStringAsBigInt, Basic) {
  EXPECT_EQ(EncodeStringAsBigInt<2>("hello"), BigInt<2>(0x68656c6c6f_Z));
  EXPECT_ASSERT(
      EncodeStringAsBigInt<2>("hello world - too long"),
      HasSubstr("String length must be at most 16"));
}

}  // namespace
}  // namespace starkware
