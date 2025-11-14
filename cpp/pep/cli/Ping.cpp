#include <pep/auth/ServerTraits.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/client/Client.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/messaging/ResponseToVoid.hpp>

#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

namespace {

class ServerPinger {
protected:
  virtual rxcpp::observable<pep::PingResponse> ping(const pep::Client& client) const = 0;
  virtual rxcpp::observable<pep::X509CertificateChain> getCertificateChain(const pep::Client& client) const = 0;

public:
  virtual ~ServerPinger() noexcept = default;

  rxcpp::observable<pep::FakeVoid> printCertificateChain(const pep::Client& client) const {
    return this->getCertificateChain(client)
      .map([](const pep::X509CertificateChain& chain) {
      auto printed = false;
      for (const auto& certificate : chain) {
        std::cout << certificate.toPem();
        printed = true;
      }
      if (!printed) {
        throw std::runtime_error("Server signed its ping response with an empty certificate chain?!?");
      }
      return pep::FakeVoid();
        });
  }

  rxcpp::observable<pep::FakeVoid> pingAndPrint(const pep::Client& client, bool drift) const {
    return this->ping(client)
      .map([drift](const pep::PingResponse& response) {
      if (drift) {
        std::cout
          << duration_cast<std::chrono::milliseconds>(
            pep::TimeNow() - response.mTimestamp
          ).count();
      } else {
        std::cout << "Received response";
      }
      std::cout << std::endl;
      return pep::FakeVoid();
        });
  }
};

template <typename TConcrete>
class ProxyPinger : public ServerPinger {
public:
  using ClientMethod = std::shared_ptr<const TConcrete>(pep::Client::*)(bool) const;

  ProxyPinger(pep::ServerTraits traits, ClientMethod method) : mTraits(std::move(traits)), mMethod(method) {}

  rxcpp::observable<pep::PingResponse> ping(const pep::Client& client) const override {
    return this->getProxy(client)->requestPing();
  }

  rxcpp::observable<pep::X509CertificateChain> getCertificateChain(const pep::Client& client) const override {
    if (mTraits.userGroups().empty()) {
      return rxcpp::observable<>::error<pep::X509CertificateChain>(std::runtime_error("This server does not produce a certificate chain to print"));
    }
    auto& signing = static_cast<const pep::SigningServerProxy&>(*this->getProxy(client)); // TODO: prevent downcast
    return signing.requestCertificateChain();
  }

private:
  std::shared_ptr<const pep::ServerProxy> getProxy(const pep::Client& client) const {
    return (client.*mMethod)(true);
  }

  pep::ServerTraits mTraits;
  ClientMethod mMethod;
};

using ServerPingers = std::map<std::string, std::shared_ptr<ServerPinger>>;

template <typename TProxy>
void AddPinger(ServerPingers& destination, pep::ServerTraits server, std::shared_ptr<const TProxy>(pep::Client::* method)(bool) const) {
  auto id = server.commandLineParameter();
  auto concrete = pep::MakeSharedCopy(ProxyPinger<TProxy>(std::move(server), method));
  [[maybe_unused]] auto emplaced = destination.emplace(id, concrete).second;
  assert(emplaced);
}

const ServerPingers serverPingers = []() { // Using a factory method to allow const initialization
  ServerPingers result;

  AddPinger(result, pep::ServerTraits::KeyServer(),           &pep::Client::getKeyServerProxy);
  AddPinger(result, pep::ServerTraits::AccessManager(),       &pep::Client::getAccessManagerProxy);
  AddPinger(result, pep::ServerTraits::StorageFacility(),     &pep::Client::getStorageFacilityProxy);
  AddPinger(result, pep::ServerTraits::Transcryptor(),        &pep::Client::getTranscryptorProxy);
  AddPinger(result, pep::ServerTraits::AuthServer(),          &pep::Client::getAuthServerProxy);
  AddPinger(result, pep::ServerTraits::RegistrationServer(),  &pep::Client::getRegistrationServerProxy);

  return result;
  }();

class CommandPing : public ChildCommandOf<CliApplication> {
public:
  explicit CommandPing(CliApplication& parent)
    : ChildCommandOf<CliApplication>("ping", "Ping a server", parent) {
  }

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    std::vector<std::string> serverIds;
    serverIds.reserve(serverPingers.size());
    std::transform(serverPingers.cbegin(), serverPingers.cend(), std::back_inserter(serverIds),
      [](const auto& pair) {return pair.first; });

    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::commandline::Parameter("server", "Server to ping").value(pep::commandline::Value<std::string>().required().allow(serverIds))
      + pep::commandline::Parameter("print-certificate-chain", "Print the server's certificate chain")
      + pep::commandline::Parameter("print-drift", "Print local time minus the server's time, in ms");
  }

  int execute() override {
    const auto& parameterValues = this->getParameterValues();
    auto pinger = serverPingers.at(parameterValues.get<std::string>("server"));

    auto printCertificateChain = parameterValues.has("print-certificate-chain");
    auto printDrift = parameterValues.has("print-drift");
    if (printDrift && printCertificateChain) {
      LOG(LOG_TAG, pep::error) << "--print-drift and --print-certificate-chain"
        << " can not be combined.";
      return 3;
    }

    return this->executeEventLoopFor(false, [pinger, printCertificateChain, printDrift](std::shared_ptr<pep::Client> client) {
      if (printCertificateChain) {
        return pinger->printCertificateChain(*client);
      }

      return pinger->pingAndPrint(*client, printDrift);
      });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandPing(CliApplication& parent) {
  return std::make_shared<CommandPing>(parent);
}
