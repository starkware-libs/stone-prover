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

#ifndef STARKWARE_ERROR_HANDLING_ERROR_HANDLING_H_
#define STARKWARE_ERROR_HANDLING_ERROR_HANDLING_H_

#include <cassert>
#include <exception>
#include <string>
#include <utility>

namespace starkware {

class StarkwareException : public std::exception {
 public:
  explicit StarkwareException(std::string message, size_t message_len)
      : message_(std::move(message)), message_len_(message_len) {}
  const char* what() const noexcept { return message_.c_str(); }  // NOLINT

  const std::string Message() const { return message_.substr(0, message_len_); }

 private:
  std::string message_;
  size_t message_len_;
};

[[noreturn]] void ThrowStarkwareException(
    const std::string& message, const char* file, size_t line_num) noexcept(false);

/*
  Overload to accept msg as char* when possible to move the std::string construction
  from the call site to the ThrowStarkwareException function.
*/
[[noreturn]] void ThrowStarkwareException(
    const char* msg, const char* file, size_t line_num) noexcept(false);

#define THROW_STARKWARE_EXCEPTION(msg) ::starkware::ThrowStarkwareException(msg, __FILE__, __LINE__)

/*
  We use "do {} while(false);" pattern to force the user to use ; after the macro.
*/
#define ASSERT_IMPL(cond, msg)        \
  do {                                \
    if (!(cond)) {                    \
      THROW_STARKWARE_EXCEPTION(msg); \
    }                                 \
  } while (false)

#ifndef NDEBUG
#define ASSERT_DEBUG(cond, msg) ASSERT_IMPL(cond, msg)
#else
#define ASSERT_DEBUG(cond, msg) \
  do {                          \
  } while (false)
#endif

#define ASSERT_RELEASE(cond, msg) ASSERT_IMPL(cond, msg)
#define ASSERT_VERIFIER(cond, msg) ASSERT_IMPL(cond, msg)

// Slow tests are tests that we don't wish to run valgrind with (because it would take too long).
#define SLOW_TEST(test_case_name, test_name) TEST(test_case_name, DISABLED_##test_name)

#define SLOW_TYPED_TEST(test_case_name, test_name) TYPED_TEST(test_case_name, DISABLED_##test_name)

#define SLOW_TEST_F(test_case_name, test_name) TEST_F(test_case_name, DISABLED_##test_name)

}  // namespace starkware

#endif  // STARKWARE_ERROR_HANDLING_ERROR_HANDLING_H_
