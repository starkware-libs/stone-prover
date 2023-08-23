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

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "starkware/error_handling/test_utils.h"

namespace starkware {

#define NO_FAIL_MESSAGE "problem : test 1 == 1 failed"
#define FAIL_MESSAGE "no problem : 1 indeed does not equal 0"

namespace {

using testing::HasSubstr;

TEST(Assert, FileAndLineNumber) {
  // Create the matcher in advance so that the EXPECT_ASSERT line will fit into one line, and thus
  // the value of __LINE__ in the message can be predicted both in clang and g++.
  auto matcher = HasSubstr(__FILE__ ":" + std::to_string(__LINE__ + 1) + ": " FAIL_MESSAGE);
  EXPECT_ASSERT(ASSERT_RELEASE(1 == 0, FAIL_MESSAGE), matcher);
  auto matcher2 = HasSubstr(__FILE__ ":" + std::to_string(__LINE__ + 1) + ": " FAIL_MESSAGE);
  EXPECT_ASSERT(THROW_STARKWARE_EXCEPTION(FAIL_MESSAGE), matcher2);
}

TEST(Assert, Debug) {
  EXPECT_ASSERT_IN_DEBUG(ASSERT_DEBUG(1 == 0, FAIL_MESSAGE), HasSubstr(FAIL_MESSAGE));
#ifndef NDEBUG
  EXPECT_NO_THROW(ASSERT_DEBUG(1 == 1, NO_FAIL_MESSAGE));
#else
  EXPECT_NO_THROW(ASSERT_DEBUG(1 == 1, "ASSERT_DEBUG throws on true in debug mode"));
  EXPECT_NO_THROW(ASSERT_DEBUG(1 == 0, "ASSERT_DEBUG is compiled in debug mode..."));
#endif
}

TEST(Assert, Release) {
  EXPECT_NO_THROW(ASSERT_RELEASE(1 == 1, NO_FAIL_MESSAGE));
  EXPECT_ASSERT(ASSERT_RELEASE(1 == 0, FAIL_MESSAGE), HasSubstr(FAIL_MESSAGE));
}

TEST(Assert, Verifier) {
  EXPECT_NO_THROW(ASSERT_VERIFIER(1 == 1, NO_FAIL_MESSAGE));
  EXPECT_ASSERT(ASSERT_VERIFIER(1 == 0, FAIL_MESSAGE), HasSubstr(FAIL_MESSAGE));
}

TEST(Assert, ExpectAssert) {
  std::string message;

  try {
    // Trigger an exception where "No such substring." appers in the stack trace but not
    // in the message.
    ASSERT_RELEASE("No such substring." == nullptr, "Actual error string.");
  } catch (const StarkwareException& e) {
    message = e.Message();
  }

  EXPECT_THAT(message, HasSubstr("Actual error string."));
  EXPECT_THAT(message, Not(HasSubstr("No such substring.")));
}

}  // namespace

}  // namespace starkware
