#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/storagefacility/PurgeCycle.hpp>

namespace pep {

// Begin (defining) a new purge cycle
struct PurgeAnnounceRequest {
  PurgeCycle::Definition definition;
};

struct PurgeAnnounceResponse {
  PurgeCycle::Id id;
};

// Cancel an (unconfirmed) purge cycle
struct PurgeCancelRequest {
  PurgeCycle::Id id;
};

struct PurgeCancelResponse {};

// Request confirmation for a purge cycle
struct PurgeReviewRequest {
  PurgeCycle::Id id;
};

struct PurgeReviewResponse {}; // Only indicates that SF has stored the request as "reviewable". A later (asynchronous) "PurgeConfirmationRequest" specifies the review's outcome

// Confirm or deny a purge cycle
struct PurgeConfirmationRequest {
  PurgeCycle::Id id;
  bool confirmed{}; // If true, SF (immediately) starts purging. If false, the request is no longer marked as "reviewable"
};

struct PurgeConfirmationResponse {};

using SignedPurgeAnnounceRequest = Signed<PurgeAnnounceRequest>;
using SignedPurgeCancelRequest = Signed<PurgeCancelRequest>;
using SignedPurgeReviewRequest = Signed<PurgeReviewRequest>;
using SignedPurgeConfirmationRequest = Signed<PurgeConfirmationRequest>;

}
