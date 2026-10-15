#pragma once

#include <pep/accessmanager/AccessManagerProxy.hpp>
#include <pep/server/ChecksumChainHandler.hpp>


namespace pep::authserver {

class ChecksumChainForwarder : public ChecksumChainHandler {
private:
  const AccessManagerProxy& am_;

public:
  explicit ChecksumChainForwarder(const AccessManagerProxy& am) noexcept : am_(am) {};

  std::set<std::string> chainNames() const;
  rxcpp::observable<ChecksumChainResponse> handle(const ChecksumChainRequest& request) const override;
};

}
