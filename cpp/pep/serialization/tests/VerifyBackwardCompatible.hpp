#pragma once

#include <pep/serialization/MessageMagic.hpp>

#include <gtest/gtest.h>

#include <string_view>

namespace pep {

template <typename T>
void VerifyBackwardCompatibleSerialization(std::string_view expected_name, pep::MessageMagic expected_magic) {
  using namespace pep;

  auto name = GetNormalizedTypeName<T>();
  EXPECT_EQ(name, expected_name);

  auto magic = MessageMagician<T>::GetMagic();
  EXPECT_EQ(magic, expected_magic);
}

}
