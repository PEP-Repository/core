#pragma once

#include <pep/auth/Signed.hpp>
#include <pep/storagefacility/PurgeTypes.hpp>

namespace pep {

// Begin (defining) a new purge
class PurgeDefinitionRequest {
private:
  std::string name_;
  bool predefined_{};

public:
  explicit PurgeDefinitionRequest(std::string name, bool predefined = true);

  const std::string& name() const& noexcept { return name_; }
  std::string&& name() && noexcept { return std::move(name_); }
  bool predefined() const noexcept { return predefined_; }
};

struct PurgeDefinitionResponse {
  PurgeId id;
};

// Cancel an (unconfirmed) purge
struct PurgeCancelRequest {
  PurgeId id;
};

struct PurgeCancelResponse {};

// Request confirmation for a purge
struct PurgeReviewRequest {
  PurgeId id;
};

struct PurgeReviewResponse {}; // Only indicates that SF has stored the request as "reviewable". A later (asynchronous) "PurgeConfirmationRequest" specifies the review's outcome

// Confirm or deny a purge
struct PurgeConfirmationRequest {
  PurgeId id;
  bool confirmed{}; // If true, SF (immediately) starts purging. If false, the request is no longer marked as "reviewable"
};

struct PurgeConfirmationResponse {};

using SignedPurgeDefinitionRequest = Signed<PurgeDefinitionRequest>;
using SignedPurgeCancelRequest = Signed< PurgeCancelRequest>;
using SignedPurgeReviewRequest = Signed<PurgeReviewRequest>;
using SignedPurgeConfirmationRequest = Signed<PurgeConfirmationRequest>;

}
