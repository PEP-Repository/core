#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/storagefacility/PurgeCycle.hpp>

namespace pep {

// No separate PurgeXyzResponse types: we return PurgeCycle instances instead

// List existing purge cycles
struct PurgeListRequest {};

// Begin (defining) a new purge cycle
struct PurgeAnnounceRequest {
  PurgeCycle::Definition definition;
};

// Cancel an (unconfirmed) purge cycle
struct PurgeCancelRequest {
  PurgeCycle::Id id;
};

// Confirm or deny a purge cycle
struct PurgeConfirmationRequest {
  PurgeCycle::Id id;
};

using SignedPurgeListRequest = Signed<PurgeListRequest>;
using SignedPurgeAnnounceRequest = Signed<PurgeAnnounceRequest>;
using SignedPurgeCancelRequest = Signed<PurgeCancelRequest>;
using SignedPurgeConfirmationRequest = Signed<PurgeConfirmationRequest>;

}
