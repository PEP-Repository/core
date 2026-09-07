#include <pep/utils/Win32Api.hpp>

#include <pep/utils/Defer.hpp>

#include <gtest/gtest.h>

#ifdef _WIN32

using namespace pep::win32api;

namespace {

TEST(GetRegistryString, REG_SZ) {
  HKEY key;
  ASSERT_EQ(::RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &key), ERROR_SUCCESS);
  PEP_DEFER(RegCloseKey(key));
  EXPECT_EQ(GetRegistryString(key, "BuildBranch"), "ge_release");
}

}

#endif
