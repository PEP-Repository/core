#pragma once

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

/// Verifies that a statement throws an exception of a specified type and with a .what() text equal to the specified message (matched case sensitively)
#define PEP_EXPECT_THROWS_MESSAGE(statement, exception_type, message) \
  PEP_EXPECT_THROWS_MESSAGE_MATCHING(statement, exception_type, ::testing::StrEq(message))

/// Verifies that a statement throws an exception of a specified type and with a .what() text containing to the specified text (matched case sensitively)
#define PEP_EXPECT_THROWS_MESSAGE_CONTAINING(statement, exception_type, substring) \
  PEP_EXPECT_THROWS_MESSAGE_MATCHING(statement, exception_type, ::testing::HasSubstr(substring))

/// Verifies that a statement throws an exception of a specified type that matches the specified condition (matcher)
#define PEP_EXPECT_THROWS_MESSAGE_MATCHING(statement, exception_type, matcher) \
  EXPECT_THAT([&] { statement; }, ::testing::ThrowsMessage<exception_type>(matcher))
