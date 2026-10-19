#pragma once

#include <pep/accessmanager/AccessManagerProxy.hpp>
#include <pep/server/ChecksumChainHandler.hpp>


namespace pep {

// authserver used to have it's own storage, but that has been moved to the
// access manager. To make sure the migration from authserver to access
// manager goes correctly, we keep the old checksum chains, but instead of
// calculating them here, we pass them on to the accessmanager.
//TODO: When the migration has succeeded, this can be removed in a following release.
class ChecksumChainForwarder : public ChecksumChainHandler {
private:
  const AccessManagerProxy& am_;

public:
  explicit ChecksumChainForwarder(const AccessManagerProxy& am) noexcept : am_(am) {};

  std::set<std::string> chainNames() const;
  rxcpp::observable<ChecksumChainResponse> handle(const ChecksumChainRequest& request) const override;
};

}
