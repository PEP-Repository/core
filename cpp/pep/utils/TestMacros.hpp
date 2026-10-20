#pragma once

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#define PEP_EXPECT_THROWS_MESSAGE_MATCHING(statement, exception_type, matcher) \
  EXPECT_THAT([&] { statement; }, ::testing::ThrowsMessage<exception_type>(matcher))

#define PEP_EXPECT_THROWS_MESSAGE_CONTAINING(statement, exception_type, substring) \
  PEP_EXPECT_THROWS_MESSAGE_MATCHING(statement, exception_type, ::testing::HasSubstr(substring))
