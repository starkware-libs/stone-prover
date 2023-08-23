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

#include "starkware/error_handling/error_handling.h"

#include <memory>
#include <sstream>
#include <string>

#ifdef __EMSCRIPTEN__  // backward-cpp doesn't compile for webassembly.
#define DISABLE_STACKTRACE
#endif

#ifndef DISABLE_STACKTRACE
#define BACKWARD_HAS_DW 1  // Enable usage of libdw from the elfutils for stack trace annotation.
#include "third_party/backward-cpp/backward.hpp"

namespace {
/*
  Prints the current stack trace to the given stream.
*/
void PrintStackTrace(std::ostream* s) {
  const size_t max_stack_depth = 64;

  backward::StackTrace st;
  st.load_here(max_stack_depth);
  backward::Printer p;
  p.print(st, *s);
}
}  // namespace

#endif

namespace starkware {

void ThrowStarkwareException(
    const std::string& message, const char* file, size_t line_num) noexcept(false) {
  // Receive file as char* to do the string construction here, rather than at the call site.
  std::stringstream s;
  s << file << ":" << line_num << ": " << message << "\n";

  size_t orig_message_len = s.tellp();
#ifndef DISABLE_STACKTRACE  // backward-cpp doesn't compile for webassembly.
  PrintStackTrace(&s);
#endif

  std::string exception_str = s.str();

  // Remove stack trace items, starting from error_handling.cc (which are usually irrelevant).
#ifndef DISABLE_STACKTRACE  // backward-cpp doesn't compile for webassembly.
  size_t first_error_handling_line_pos =
      exception_str.find("src/starkware/error_handling/error_handling.cc\", line ");
  if (first_error_handling_line_pos != std::string::npos) {
    size_t line_start_pos = exception_str.rfind('\n', first_error_handling_line_pos);
    if (line_start_pos != std::string::npos) {
      exception_str = exception_str.substr(0, line_start_pos);
    }
  }
#endif

  throw StarkwareException(exception_str, orig_message_len);
}

void ThrowStarkwareException(const char* msg, const char* file, size_t line_num) noexcept(false) {
  const std::string message(msg);
  ThrowStarkwareException(message, file, line_num);
}

}  // namespace starkware
