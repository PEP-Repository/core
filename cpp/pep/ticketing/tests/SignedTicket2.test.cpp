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

  /* SignedTicket2 and SignedTicketRequest2 used to be standalone classes, making them inconvenient for templated
   * processing. E.g. when generic message signing was added to the Client class, we needed a separate overload
   * for the unrelated SignedTicketRequest2 class:
   * 
   * class Client {
   *   template <typename T> Signed<T> sign(T message) { ... }   // What we wanted to add
   *   SignedTicketRequest2 sign(TicketRequest2 message) { ... } // Also required to make things work
   * };
   *
   * See https://gitlab.pep.cs.ru.nl/pep/core/-/commit/c86cca366e023322a6cf2895362fd6562e280ab2#635ab8e17c7435817d081539435759a664921159_779_789
   * 
   * To allow all SignedXyz classes to be processed by unspecialized templated code, the SignedTicket2 and
   * SignedTicketRequest2 classes were later turned into aliases for (explicit specializations of) the corresponding
   * Signed<T> templates. This unit test verifies that the aliases behave the same as the original classes.
   * The expected names and MessageMagic values were extracted from the original classes in commit d9e9581.
   */
  VerifyBackwardCompatibleProperties<SignedTicket2>("SignedTicket2", 3936116042);
  VerifyBackwardCompatibleProperties<SignedTicketRequest2>("SignedTicketRequest2", 1911144167);
}
