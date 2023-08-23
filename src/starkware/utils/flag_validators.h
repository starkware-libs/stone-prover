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

#ifndef STARKWARE_UTILS_FLAG_VALIDATORS_H_
#define STARKWARE_UTILS_FLAG_VALIDATORS_H_

#include <string>

#include "glog/logging.h"

namespace starkware {

/*
  Returns true if the file file_name (given to the flag: flagname) is readable, and false otherwise.
*/
bool ValidateInputFile(const char* flagname, const std::string& file_name);

/*
  Returns true if the file file_name (given to the flag: flagname) is writable, and false otherwise.
*/
bool ValidateOutputFile(const char* flagname, const std::string& file_name);

/*
  Returns true if the file file_name (given to the flag: flagname) is either empty or readable, and
  false otherwise.
*/
bool ValidateOptionalInputFile(const char* flagname, const std::string& file_name);

/*
  Returns true if the file file_name (given to the flag: flagname) is either empty or writable, and
  false otherwise.
*/
bool ValidateOptionalOutputFile(const char* flagname, const std::string& file_name);

}  // namespace starkware

#endif  // STARKWARE_UTILS_FLAG_VALIDATORS_H_
