#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/client/Client.hpp>
#include <pep/messaging/MessagingSerializers.hpp>

#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

namespace {

class ServerPinger {
public:
  virtual ~ServerPinger() noexcept = default;
  virtual rxcpp::observable<pep::FakeVoid> execute(const pep::Client& client) const = 0;
};

template <typename TResponse>
class ServerPingerWithResponse : public ServerPinger {
protected:

  virtual rxcpp::observable<TResponse> send(const pep::Client& client) const = 0;

  virtual void handleResponse(const TResponse& response) const {
    std::cout << "Received response" << std::endl;
  }

public:
  rxcpp::observable<pep::FakeVoid> execute(const pep::Client& client) const override {
    return this->send(client)
      .map([this](const TResponse& response) {
      handleResponse(response);
      return pep::FakeVoid();
    });
  }
};

class SigningServerPinger : public ServerPingerWithResponse<pep::SignedPingResponse> {
private:
  bool mPrintCertificateChain;
  bool mPrintDrift;

protected:
  virtual std::shared_ptr<const pep::SigningServerClient> getSigningServerClient(const pep::Client& client) const = 0;

  rxcpp::observable<pep::SignedPingResponse> send(const pep::Client& client) const override {
    return this->getSigningServerClient(client)->requestPing();
  }

  void handleResponse(const pep::SignedPingResponse& response) const override {
    if (mPrintDrift) {
      std::cout
        << pep::Timestamp().getTime()
        - response.openWithoutCheckingSignature().mTimestamp.getTime()
        << std::endl;
      return;
    }

    if (!mPrintCertificateChain) {
      return ServerPingerWithResponse<pep::SignedPingResponse>::handleResponse(response);
    }

    auto printed = false;
    for (const auto& certificate : response.mSignature.mCertificateChain) {
      std::cout << certificate.toPem();
      printed = true;
    }
    if (!printed) {
      throw std::runtime_error("Server signed its ping response with an empty certificate chain?!?");
    }
    std::cout << std::endl;
  }

  SigningServerPinger(bool printCertificateChain, bool printDrift)
    : mPrintCertificateChain(printCertificateChain), mPrintDrift(printDrift) {
  }
};

template <typename TClient, typename TSigningServerClient>
class TypedSigningServerPinger : public SigningServerPinger {
  static_assert(std::is_base_of_v<TClient, pep::Client>, "TClient must be pep::Client or one of its public ancestors");

public:
  using GetSigningServerClientMethod = std::shared_ptr<const TSigningServerClient>(TClient::*)(bool) const;

private:
  GetSigningServerClientMethod mGetSigningServer;

protected:
  std::shared_ptr<const pep::SigningServerClient> getSigningServerClient(const pep::Client& client) const override {
    return (client.*mGetSigningServer)(true);
  }

public:
  TypedSigningServerPinger(GetSigningServerClientMethod getSigningServer, bool printCertificateChain, bool printDrift)
    : SigningServerPinger(printCertificateChain, printDrift), mGetSigningServer(getSigningServer) {
  }
};

class KeyServerPinger : public ServerPingerWithResponse<pep::PingResponse> {
protected:
  rxcpp::observable<pep::PingResponse> send(const pep::Client& client) const override {
    return client.getKeyClient()->requestPing();
  }

public:
  KeyServerPinger(bool printCertificateChain, bool printDrift) {
    if (printCertificateChain) {
      throw std::runtime_error("This server does not produce a certificate chain to print");
    }
    if (printDrift) {
      throw std::runtime_error("This server does not support printing drift");
      // The templated nature of this code makes it difficult, and not at all
      // worth-while, to add general drift printing code.
    }
  }
};

using ServerPingerFactory = std::function<std::shared_ptr<ServerPinger>(bool printCertificateChain, bool printDrift)>;

template <typename TClient, typename TSigningServerClient>
ServerPingerFactory MakeSigningServerPingerFactory(std::shared_ptr<const TSigningServerClient>(TClient::*getSigningServerClient)(bool) const) {
  return [getSigningServerClient](bool printCertificateChain, bool printDrift) {
    return std::make_shared<TypedSigningServerPinger<TClient, TSigningServerClient>>(getSigningServerClient, printCertificateChain, printDrift);
    };
}

const std::unordered_map<std::string, ServerPingerFactory> pingerFactories = []() { // Using a factory method to allow const initialization
  std::unordered_map<std::string, ServerPingerFactory> result;

  result["keyserver"]           = [](bool printCertificateChain, bool printDrift) { return std::make_shared<KeyServerPinger>(printCertificateChain, printDrift); };

  result["accessmanager"]       = MakeSigningServerPingerFactory(&pep::CoreClient::getAccessManagerClient);
  result["storagefacility"]     = MakeSigningServerPingerFactory(&pep::CoreClient::getStorageClient);
  result["transcryptor"]        = MakeSigningServerPingerFactory(&pep::CoreClient::getTranscryptorClient);

  result["authserver"]          = MakeSigningServerPingerFactory(&pep::Client::getAuthClient);
  result["registrationserver"]  = MakeSigningServerPingerFactory(&pep::Client::getRegistrationClient);

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
      [](const std::pair<const std::string, ServerPingerFactory>& pair) {return pair.first; });

    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::commandline::Parameter("server", "Server to ping").value(pep::commandline::Value<std::string>().required().allow(serverIds))
      + pep::commandline::Parameter("print-certificate-chain", "Print the server's certificate chain")
      + pep::commandline::Parameter("print-drift", "Print local time minus the server's time, in ms");
  }

  int execute() override {
    const auto& parameterValues = this->getParameterValues();

    auto factory = pingerFactories.find(parameterValues.get<std::string>("server"));
    assert(factory != pingerFactories.cend());

    auto printCertificateChain = parameterValues.has("print-certificate-chain");
    auto printDrift = parameterValues.has("print-drift");
    if (printDrift && printCertificateChain) {
      LOG(LOG_TAG, pep::error) << "--print-drift and --print-certificate-chain"
        << " can not be combined.";
      return 3;
    }

    auto pinger = factory->second(printCertificateChain, printDrift);

    return this->executeEventLoopFor(false,
      [pinger](std::shared_ptr<pep::Client> client) {
        return pinger->execute(*client);
      });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandPing(CliApplication& parent) {
  return std::make_shared<CommandPing>(parent);
}
