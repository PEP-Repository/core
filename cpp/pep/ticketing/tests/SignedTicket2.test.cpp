#include <pep/ticketing/TicketingMessages.hpp>
#include <pep/utils/TestCompatibleMessageMagic.hpp>
#include <gtest/gtest.h>

TEST(SignedTicket2, ClassesHaveBackwardCompatibleSerialization) {
  using namespace pep;

  /* SignedTicket2 and SignedTicketRequest2 used to be standalone classes, making them inconvenient for templated
   * processing. E.g. when generic message signing was added, we needed a separate overload because SignedTicketRequest2
   * wasn't a specialization of Signed<T>:
   *
   * class Client {
   *   template <typename T> Signed<T> sign(T message) { ... }   // What we wanted to add
   *   SignedTicketRequest2 sign(TicketRequest2 message) { ... } // Also required to make things work
   * };
   *
   * See https://gitlab.pep.cs.ru.nl/pep/core/-/commit/c86cca366e023322a6cf2895362fd6562e280ab2#635ab8e17c7435817d081539435759a664921159_779_789
   *
   * To correct this, the SignedTicket2 and SignedTicketRequest2 classes were later turned into aliases for
   * (explicit specializations of) the corresponding Signed<T> templates. This unit test verifies that the aliases
   * are compatible with the original classes, so that previously serialized (and possibly stored) SignedTicket2
   * instances can be deserialized to the new Signed<Ticket2> type.
   *
   * The expected names and MessageMagic values were extracted from the original classes in commit d9e9581.
   */
  VerifyBackwardCompatibleSerialization<SignedTicket2>("SignedTicket2", 3936116042);
  VerifyBackwardCompatibleSerialization<SignedTicketRequest2>("SignedTicketRequest2", 1911144167);
}
