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

#ifndef STARKWARE_STL_UTILS_STRING_H_
#define STARKWARE_STL_UTILS_STRING_H_

#include <string>
#include <vector>

inline std::vector<std::string> Split(const std::string& str, const char delimiter) {
  std::vector<std::string> ret_vec;
  if (str.empty()) {
    return ret_vec;
  }
  size_t element_pos = 0;
  while (true) {
    size_t delimiter_position = str.find(delimiter, element_pos);
    ret_vec.push_back(str.substr(element_pos, delimiter_position - element_pos));
    if (delimiter_position == std::string::npos) {
      break;
    }
    element_pos = delimiter_position + 1;
  }

  return ret_vec;
}

#endif  // STARKWARE_STL_UTILS_STRING_H_
