#include <pep/storagefacility/EntryName.hpp>

#include <gtest/gtest.h>

using namespace pep;

namespace {

TEST(EntryName, format) {
  EXPECT_EQ(std::format("[{}]", EntryName("participant", "column")), "[participant/column]");
}

}
