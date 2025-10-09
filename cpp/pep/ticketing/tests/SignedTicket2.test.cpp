#include <pep/ticketing/TicketingMessages.hpp>
#include <gtest/gtest.h>

namespace {

template <typename T>
void VerifyBackwardCompatibleProperties(const char* expected_name, pep::MessageMagic expected_magic) {
  using namespace pep;

  auto name = GetNormalizedTypeName<T>();
  EXPECT_EQ(name, expected_name);

  auto magic = MessageMagician<T>::GetMagic();
  EXPECT_EQ(magic, expected_magic);
}

}

TEST(SignedTicket2, ClassesHaveBackwardCompatibleProperties) {
  using namespace pep;

  // Reference values calculated on standalone classes in commit d9e9581
  VerifyBackwardCompatibleProperties<SignedTicket2>("SignedTicket2", 3936116042);
  VerifyBackwardCompatibleProperties<SignedTicketRequest2>("SignedTicketRequest2", 1911144167);
}
