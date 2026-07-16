#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/storagefacility/PurgeTypes.hpp>

namespace pep {

// Begin (defining) a new purge cycle
class PurgeCycleRequest {
private:
  std::string name_;
  bool predefined_{};

public:
  explicit PurgeCycleRequest(std::string name, bool predefined = true);

  const std::string& name() const& noexcept { return name_; }
  std::string&& name() && noexcept { return std::move(name_); }
  bool predefined() const noexcept { return predefined_; }
};

struct PurgeCycleResponse {
  PurgeCycleId id;
};

// Cancel an (unconfirmed) purge cycle
struct PurgeCancelRequest {
  PurgeCycleId id;
};

struct PurgeCancelResponse {};

// Request confirmation for a purge cycle
struct PurgeReviewRequest {
  PurgeCycleId id;
};

struct PurgeReviewResponse {}; // Only indicates that SF has stored the request as "reviewable". A later (asynchronous) "PurgeConfirmationRequest" specifies the review's outcome

// Confirm or deny a purge cycle
struct PurgeConfirmationRequest {
  PurgeCycleId id;
  bool confirmed{}; // If true, SF (immediately) starts purging. If false, the request is no longer marked as "reviewable"
};

struct PurgeConfirmationResponse {};

using SignedPurgeCycleRequest = Signed<PurgeCycleRequest>;
using SignedPurgeCancelRequest = Signed< PurgeCancelRequest>;
using SignedPurgeReviewRequest = Signed<PurgeReviewRequest>;
using SignedPurgeConfirmationRequest = Signed<PurgeConfirmationRequest>;

}
