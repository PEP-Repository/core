#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/client/Client.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/messaging/ResponseToVoid.hpp>

#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

namespace {

class ServerPinger {
private:
  const pep::Client& mClient;

protected:
  const pep::Client& client() const noexcept { return mClient; }

  static void HandleResponse(const pep::PingResponse& response, bool printDrift) {
    if (printDrift) {
      std::cout
        << duration_cast<std::chrono::milliseconds>(
          pep::TimeNow() - response.mTimestamp
        ).count();
    }
    else {
      std::cout << "Received response";
    }
    std::cout << std::endl;
  }

public:
  using Factory = std::function<std::shared_ptr<ServerPinger>(const pep::Client&)>;

  explicit ServerPinger(const pep::Client& client) noexcept : mClient(client) {}
  virtual ~ServerPinger() noexcept = default;

  virtual rxcpp::observable<pep::FakeVoid> ping(bool printCertificateChain, bool printDrift) const = 0;
};


class SigningServerPinger : public ServerPinger {
public:
  using GetProxyFunction = std::function<std::shared_ptr<const pep::SigningServerProxy>(const pep::Client&)>;

private:
  GetProxyFunction mGetProxy;

  static pep::FakeVoid PrintCertificateChain(const pep::SignedPingResponse& response) {
    auto printed = false;
    for (const auto& certificate : response.mSignature.mCertificateChain) {
      std::cout << certificate.toPem();
      printed = true;
    }
    if (!printed) {
      throw std::runtime_error("Server signed its ping response with an empty certificate chain?!?");
    }
    return pep::FakeVoid();
  }

  SigningServerPinger(const pep::Client& client, GetProxyFunction getProxy)
    : ServerPinger(client), mGetProxy(std::move(getProxy)) {
  }

public:
  template <class TProxy, class TClient>
  static Factory MakeFactory(std::shared_ptr<const TProxy>(TClient::* method)(bool) const) {
    static_assert(std::is_base_of_v<TClient, pep::Client>, "TClient must be pep::Client or one of its public base classes");
    static_assert(std::is_base_of_v<pep::SigningServerProxy, TProxy>, "TProxy must inherit from (or be) SigningServerProxy");

    return [method](const pep::Client& client) {
      auto getProxy = [method](const pep::Client& client) -> std::shared_ptr<const pep::SigningServerProxy> { return (client.*method)(true); };
      return std::shared_ptr<SigningServerPinger>(new SigningServerPinger(client, std::move(getProxy)));
      };
  }

  rxcpp::observable<pep::FakeVoid> ping(bool printCertificateChain, bool printDrift) const override {
    std::function<void(const pep::SignedPingResponse&)> handle;
    if (printDrift || !printCertificateChain) {
      handle = [printDrift](const pep::SignedPingResponse& response) { HandleResponse(response.openWithoutCheckingSignature(), printDrift); };
    }
    else { // printCertificateChain
      handle = [](const pep::SignedPingResponse& response) { PrintCertificateChain(response); };
    }

    return mGetProxy(this->client())->requestPing()
      .tap(handle)
      .op(pep::messaging::ResponseToVoid<true>());
  };
};


class KeyServerPinger : public ServerPinger {
public:
  using ServerPinger::ServerPinger;

  rxcpp::observable<pep::FakeVoid> ping(bool printCertificateChain, bool printDrift) const override {
    if (printCertificateChain) {
      throw std::runtime_error("This server does not produce a certificate chain to print");
    }
    return this->client().getKeyServerProxy()->requestPing()
      .tap([printDrift](const pep::PingResponse& response) { HandleResponse(response, printDrift); })
      .op(pep::messaging::ResponseToVoid<true>());
  }
};


const std::unordered_map<std::string, ServerPinger::Factory> pingerFactories = []() { // Using a factory method to allow const initialization
  std::unordered_map<std::string, ServerPinger::Factory> result;

  result["keyserver"] = [](const pep::Client& client) { return std::make_shared<KeyServerPinger>(client); };

  result["accessmanager"] = SigningServerPinger::MakeFactory(&pep::CoreClient::getAccessManagerProxy);
  result["storagefacility"] = SigningServerPinger::MakeFactory(&pep::CoreClient::getStorageFacilityProxy);
  result["transcryptor"] = SigningServerPinger::MakeFactory(&pep::CoreClient::getTranscryptorProxy);

  result["authserver"] = SigningServerPinger::MakeFactory(&pep::Client::getAuthServerProxy);
  result["registrationserver"] = SigningServerPinger::MakeFactory(&pep::Client::getRegistrationServerProxy);

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
    serverIds.reserve(pingerFactories.size());
    std::transform(pingerFactories.cbegin(), pingerFactories.cend(), std::back_inserter(serverIds),
      [](const std::pair<const std::string, ServerPinger::Factory>& pair) {return pair.first; });

    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::commandline::Parameter("server", "Server to ping").value(pep::commandline::Value<std::string>().required().allow(serverIds))
      + pep::commandline::Parameter("print-certificate-chain", "Print the server's certificate chain")
      + pep::commandline::Parameter("print-drift", "Print local time minus the server's time, in ms");
  }

  int execute() override {
    const auto& parameterValues = this->getParameterValues();

    auto found = pingerFactories.find(parameterValues.get<std::string>("server"));
    assert(found != pingerFactories.cend());

    auto printCertificateChain = parameterValues.has("print-certificate-chain");
    auto printDrift = parameterValues.has("print-drift");
    if (printDrift && printCertificateChain) {
      LOG(LOG_TAG, pep::error) << "--print-drift and --print-certificate-chain"
        << " can not be combined.";
      return 3;
    }

    return this->executeEventLoopFor(false,
      [factory = found->second, printCertificateChain, printDrift](std::shared_ptr<pep::Client> client) {
        return factory(*client)->ping(printCertificateChain, printDrift);
      });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandPing(CliApplication& parent) {
  return std::make_shared<CommandPing>(parent);
}
