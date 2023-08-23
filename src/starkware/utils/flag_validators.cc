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

#include "starkware/utils/flag_validators.h"

#include <fstream>
#include <string>

#include "glog/logging.h"

namespace starkware {

inline bool FileExists(const std::string& filename) {
  std::ifstream ifile(filename);
  return ifile.good();
}

bool ValidateInputFile(const char* /*flagname*/, const std::string& file_name) {
  std::ifstream file(file_name);
  return static_cast<bool>(file);
}

bool ValidateOutputFile(const char* /*flagname*/, const std::string& file_name) {
  bool file_existed = FileExists(file_name);
  std::ofstream file(file_name, std::ofstream::app);
  bool can_write_file = static_cast<bool>(file);
  file.close();

  if (!file_existed && can_write_file) {
    std::remove(file_name.c_str());
  }
  return can_write_file;
}

bool ValidateOptionalInputFile(const char* flagname, const std::string& file_name) {
  return (file_name.empty() || ValidateInputFile(flagname, file_name));
}

bool ValidateOptionalOutputFile(const char* flagname, const std::string& file_name) {
  return (file_name.empty() || ValidateOutputFile(flagname, file_name));
}

}  // namespace starkware
