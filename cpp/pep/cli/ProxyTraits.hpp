#pragma once

#include <pep/auth/ServerTraits.hpp>
#include <pep/client/Client.hpp>

namespace pep::cli {

class ProxyTraits {
public:
  using GetProxy = std::function<std::shared_ptr<const ServerProxy>(const Client&)>;

  const ServerTraits& server() const noexcept { return mServer; }
  std::shared_ptr<const ServerProxy> getProxy(const Client& client) const;

  ProxyTraits(ServerTraits server, GetProxy getProxy) noexcept;
  static std::vector<ProxyTraits> All();

private:
  ServerTraits mServer;
  GetProxy mGetProxy;
};

}
