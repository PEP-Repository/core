#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/storagefacility/RedactionCycle.hpp>

namespace pep {

// No separate RedactionXyzResponse types: we return RedactionCycle instances instead

// List existing redaction cycles
struct RedactionListRequest {};

// Begin (defining) a new redaction cycle
struct RedactionAnnounceRequest {
  RedactionCycle::Definition definition;
};

// Cancel a redaction cycle (that has not been confirmed/applied/started yet)
struct RedactionCancelRequest {
  RedactionCycle::Id id;
};

// Confirm (and apply/start) a redaction cycle
struct RedactionConfirmationRequest {
  RedactionCycle::Id id;
};

using SignedRedactionListRequest = Signed<RedactionListRequest>;
using SignedRedactionAnnounceRequest = Signed<RedactionAnnounceRequest>;
using SignedRedactionCancelRequest = Signed<RedactionCancelRequest>;
using SignedRedactionConfirmationRequest = Signed<RedactionConfirmationRequest>;

}
