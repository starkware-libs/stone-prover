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

#ifndef STARKWARE_ERROR_HANDLING_TEST_UTILS_H_
#define STARKWARE_ERROR_HANDLING_TEST_UTILS_H_

#include <string>

#include "gmock/gmock.h"

#include "starkware/error_handling/error_handling.h"

namespace starkware {

// Below implementation is an altered version of EXPECT_THROW from GTest.
#define EXPECT_THROW_MSG(statement, expected_matcher)                     \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                           \
  if (::testing::internal::ConstCharPtr gtest_msg = "") {                 \
    bool gtest_caught_expected = false;                                   \
    std::string message__;                                                \
    try {                                                                 \
      GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);          \
    } catch (const StarkwareException& e) {                               \
      gtest_caught_expected = true;                                       \
      message__ = e.Message();                                            \
    } catch (...) {                                                       \
      gtest_msg.value = "Expected: " #statement                           \
                        " throws an exception of type StarkwareException" \
                        ".\n  Actual: it throws a different type.";       \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__);         \
    }                                                                     \
    if (gtest_caught_expected) {                                          \
      EXPECT_THAT(message__, expected_matcher);                           \
    } else {                                                              \
      gtest_msg.value = "Expected: " #statement                           \
                        " throws an exception of type StarkwareException" \
                        ".\n  Actual: it throws nothing.";                \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__);         \
    }                                                                     \
  } else {                                                                \
    GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__)                 \
        : GTEST_NONFATAL_FAILURE_(gtest_msg.value);                       \
  }

// This is used to test that our assertions were thrown with appropriate messages.
#ifndef NDEBUG
#define EXPECT_ASSERT_IN_DEBUG(statement, msg) EXPECT_THROW_MSG(statement, msg)
#else
#define EXPECT_ASSERT_IN_DEBUG(statement, msg) \
  do {                                         \
  } while (false)
#endif

#define EXPECT_ASSERT(statement, msg) EXPECT_THROW_MSG(statement, msg)

}  // namespace starkware

#endif  // STARKWARE_ERROR_HANDLING_TEST_UTILS_H_
