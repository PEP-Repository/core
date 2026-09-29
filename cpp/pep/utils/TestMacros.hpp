#pragma once

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#define PEP_EXPECT_THROWS_MESSAGE(statement, exception_type, ...) \
  EXPECT_THAT([&] { statement; }, ::testing::ThrowsMessage<exception_type>(::testing::AllOf(__VA_ARGS__)))
