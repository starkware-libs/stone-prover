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

#ifndef STARKWARE_UTILS_TO_FROM_STRING_H_
#define STARKWARE_UTILS_TO_FROM_STRING_H_

#include <cstddef>
#include <iomanip>
#include <string>
#include <vector>

#include "third_party/gsl/gsl-lite.hpp"

namespace starkware {

/*
  Converts an array of bytes to an ASCII string. Leading zero's are trimmed.
  [00, 2B, AA, 10] -> "0x2BAA10".
*/
std::string BytesToHexString(gsl::span<const std::byte> data, bool trim_leading_zeros = true);

/*
  Convert an ASCII string to an array of bytes.
  The input string is left padded with zero's to match as_bytes_out.size()
  0x2BAA10 -> [00, 2B, AA, 10] (assuming as_bytes_out.size() == 4).
*/
void HexStringToBytes(const std::string& hex_string, gsl::span<std::byte> as_bytes_out);

/*
  Parses a string representing a 64 bit non-negative integer.
  The string must represent the number in the canonical representation.
  In particular no leading zeros are allowed (except for the '0' string which is legal).
*/
uint64_t StrToUint64(const std::string& str);

}  // namespace starkware

#endif  // STARKWARE_UTILS_TO_FROM_STRING_H_
