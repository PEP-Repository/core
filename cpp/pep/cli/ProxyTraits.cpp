#include <pep/cli/ProxyTraits.hpp>

namespace pep::cli {

namespace {

template <typename TProxy>
ProxyTraits MakeProxyTraits(ServerTraits server, std::shared_ptr<const TProxy>(Client::* method)(bool) const) {
  return ProxyTraits(std::move(server), [method](const Client& client) -> std::shared_ptr<const ServerProxy> {
    return (client.*method)(true);
    });
}

}

std::shared_ptr<const ServerProxy> ProxyTraits::getProxy(const Client& client) const {
  return mGetProxy(client);
}

ProxyTraits::ProxyTraits(ServerTraits server, GetProxy getProxy) noexcept
  : mServer(std::move(server)), mGetProxy(std::move(getProxy)) {
}

std::vector<ProxyTraits> ProxyTraits::All() {
  return {
    MakeProxyTraits(ServerTraits::AccessManager(),      &Client::getAccessManagerProxy),
    MakeProxyTraits(ServerTraits::AuthServer(),         &Client::getAuthServerProxy),
    MakeProxyTraits(ServerTraits::KeyServer(),          &Client::getKeyServerProxy),
    MakeProxyTraits(ServerTraits::RegistrationServer(), &Client::getRegistrationServerProxy),
    MakeProxyTraits(ServerTraits::StorageFacility(),    &Client::getStorageFacilityProxy),
    MakeProxyTraits(ServerTraits::Transcryptor(),       &Client::getTranscryptorProxy),
  };
}

}
