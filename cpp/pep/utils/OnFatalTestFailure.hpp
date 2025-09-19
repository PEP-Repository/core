#pragma once

#include <pep/utils/Defer.hpp>
#include <gtest/gtest.h>

// Runs a function when (the current scope ends and) a fatal test failure has occurred
#define PEP_ON_FATAL_TEST_FAILURE(code)   PEP_DEFER(if (::testing::Test::HasFatalFailure()) {code;})
