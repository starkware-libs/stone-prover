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

#include "starkware/utils/to_from_string.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

#include "starkware/error_handling/error_handling.h"

namespace starkware {

std::string BytesToHexString(gsl::span<const std::byte> data, bool trim_leading_zeros) {
  ASSERT_RELEASE(!data.empty(), "Cannot convert empty byte sequence to hex.");
  std::stringstream s;
  auto iter = data.begin();
  s << "0x";
  if (trim_leading_zeros) {
    // Do not write leading zeros to string.
    for (; iter != data.end() && std::to_integer<int>(*iter) == 0; iter++) {
    }
    // If all bytes are zero, output a single zero.
    if (iter == data.end()) {
      return "0x0";
    }
    // Otherwise - write them as hex characters, adding leading 0s to all but the first byte.
    s << std::hex << std::to_integer<int>(*iter++);
  }

  for (; iter != data.end(); iter++) {
    s << std::setfill('0') << std::setw(2) << std::hex << std::to_integer<int>(*iter);
  }

  return s.str();
}

void HexStringToBytes(const std::string& hex_string, gsl::span<std::byte> as_bytes_out) {
  ASSERT_RELEASE(
      hex_string.length() > 2,
      "String (\"" + hex_string + "\") is too short, expected at least two chars (for 0x)");
  const std::string hex_prefix = "0x";
  auto iter_pair =
      std::mismatch(hex_prefix.begin(), hex_prefix.end(), hex_string.begin(), hex_string.end());
  ASSERT_RELEASE(
      hex_prefix.end() == iter_pair.first,
      "String (\"" + hex_string + "\") does not start with '0x'.");
  std::string pure_hex_representation = hex_string.substr(2);
  // Trim all leading zeros.
  pure_hex_representation.erase(
      0,
      std::min(pure_hex_representation.find_first_not_of('0'), pure_hex_representation.size() - 1));
  // Prepend one zero if the string is of odd length, to make parsing easier (that way, 1byte = two
  // characters in the string).
  if (pure_hex_representation.length() % 2 != 0) {
    pure_hex_representation.insert(0, 1, '0');
  }

  // Ignoring the initial '0x', the output's size should be at least half the hex's size, since
  // each pair of characters map to a byte.
  ASSERT_RELEASE(
      as_bytes_out.size() >= pure_hex_representation.length() / 2,
      "Output span's length (" + std::to_string(as_bytes_out.size()) +
          ") is smaller than half of the pure hex number's length (" +
          std::to_string(pure_hex_representation.length() / 2) + ").");
  // If the byte span is larger, we'll fill it up with zeros later, and use the necessary offset.
  size_t offset = as_bytes_out.size() - pure_hex_representation.length() / 2;
  // Read pairs of characters and try to convert them to a byte.
  for (size_t i = 0; i < pure_hex_representation.length() / 2; i++) {
    std::stringstream str;
    str << std::hex << pure_hex_representation.substr(i * 2, 2);
    int c;
    str >> c;
    as_bytes_out[offset + i] = std::byte(c);
  }
  // Pad with zeros.
  std::fill(as_bytes_out.begin(), as_bytes_out.begin() + offset, std::byte{0});
}

uint64_t StrToUint64(const std::string& str) {
  uint64_t res = 0;
  std::istringstream iss(str);
  iss >> res;
  ASSERT_RELEASE(
      str == std::to_string(res),
      "Input string (\"" + str + "\") does not represent a valid uint64_t value.");
  return res;
}

}  // namespace starkware
