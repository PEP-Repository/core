#pragma once

#include <pep/ticketing/TicketingMessages.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>
#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(LocalPseudonyms);
PEP_DEFINE_CODED_SERIALIZER(Ticket2);
PEP_DEFINE_CODED_SERIALIZER(SignedTicket2);

PEP_DEFINE_CODED_SERIALIZER(TicketRequest2);
PEP_DEFINE_CODED_SERIALIZER(SignedTicketRequest2);

}
