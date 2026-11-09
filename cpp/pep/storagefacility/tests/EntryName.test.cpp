#include <pep/storagefacility/EntryName.hpp>

#include <gtest/gtest.h>

#include <format>

using namespace pep;

namespace {

TEST(EntryName, format) {
  const auto pseudonym = LocalPseudonym::Random();
  EXPECT_EQ(std::format("[{}]", EntryName(pseudonym, "column")), std::format("[{}/column]", pseudonym.text()));
}

}
