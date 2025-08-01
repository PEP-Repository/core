#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/client/Client.hpp>
#include <pep/networking/NetworkingSerializers.hpp>

#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

namespace {

class ServerPinger {
public:
  virtual ~ServerPinger() noexcept = default;
  virtual rxcpp::observable<pep::FakeVoid> execute(const pep::Client& client) const = 0;
};

template <typename TClient, typename TResponse>
class TypedServerPinger : public ServerPinger {
public:
  using SendMethod = rxcpp::observable<TResponse>(TClient::*)() const;

private:
  SendMethod mSend;

protected:
  TypedServerPinger(SendMethod send) : mSend(send) {}

  virtual void handleResponse(const TResponse& response) const {
    std::cout << "Received response" << std::endl;
  }

public:
  rxcpp::observable<pep::FakeVoid> execute(const pep::Client& client) const override {
    return (client.*mSend)()
      .map([this](const TResponse& response) {
      handleResponse(response);
      return pep::FakeVoid();
    });
  }
};

template <typename TClient, rxcpp::observable<pep::SignedPingResponse> (TClient::*SendRequestMethod)() const>
class SigningServerPinger : public TypedServerPinger<TClient, pep::SignedPingResponse> {
private:
  bool mPrintCertificateChain;
  bool mPrintDrift;

  void handleResponse(const pep::SignedPingResponse& response) const override {
    if (mPrintDrift) {
      std::cout 
        << pep::Timestamp().getTime()
             - response.openWithoutCheckingSignature().mTimestamp.getTime()
        << std::endl;
      return;
    }

    if (!mPrintCertificateChain) {
      return TypedServerPinger<TClient, pep::SignedPingResponse>::handleResponse(response);
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

public:
  SigningServerPinger(bool printCertificateChain, bool printDrift) 
    : TypedServerPinger<TClient, pep::SignedPingResponse>(SendRequestMethod), 
      mPrintCertificateChain(printCertificateChain),
      mPrintDrift(printDrift) {}
};

class KeyServerPinger : public TypedServerPinger<pep::Client, pep::PingResponse> {
public:
  KeyServerPinger(bool printCertificateChain, bool printDrift) 
      : TypedServerPinger<pep::Client, pep::PingResponse>(&pep::Client::pingKeyServer) {
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

using AccessManagerPinger = SigningServerPinger<pep::CoreClient, &pep::CoreClient::pingAccessManager>;
using StorageFacilityPinger = SigningServerPinger<pep::CoreClient, &pep::CoreClient::pingStorageFacility>;
using TranscryptorPinger = SigningServerPinger<pep::CoreClient, &pep::CoreClient::pingTranscryptor>;
using AuthServerPinger = SigningServerPinger<pep::Client, &pep::Client::pingAuthserver>;
using RegistrationServerPinger = SigningServerPinger<pep::Client, &pep::Client::pingRegistrationServer>;

template <typename TPinger>
std::shared_ptr<ServerPinger> CreateServerPinger(bool printCertificateChain, bool printDrift) {
  return std::make_shared<TPinger>(printCertificateChain, printDrift);
}

using ServerPingerFactoryMethod = std::shared_ptr<ServerPinger> (*)(bool printCertificateChain, bool printDrift);

const std::unordered_map<std::string, ServerPingerFactoryMethod> pingerFactoryMethods = []() { // Using a factory method to allow const initialization
  std::unordered_map<std::string, ServerPingerFactoryMethod> result;

  result["accessmanager"] = &CreateServerPinger<AccessManagerPinger>;
  result["keyserver"] = &CreateServerPinger<KeyServerPinger>;
  result["registrationserver"] = &CreateServerPinger<RegistrationServerPinger>;
  result["storagefacility"] = &CreateServerPinger<StorageFacilityPinger>;
  result["transcryptor"] = &CreateServerPinger<TranscryptorPinger>;
  result["authserver"] = &CreateServerPinger<AuthServerPinger>;

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
    serverIds.reserve(pingerFactoryMethods.size());
    std::transform(pingerFactoryMethods.cbegin(), pingerFactoryMethods.cend(), std::back_inserter(serverIds),
      [](const std::pair<const std::string, ServerPingerFactoryMethod>& pair) {return pair.first; });

    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::commandline::Parameter("server", "Server to ping").value(pep::commandline::Value<std::string>().required().allow(serverIds))
      + pep::commandline::Parameter("print-certificate-chain", "Print the server's certificate chain")
      + pep::commandline::Parameter("print-drift", "Print local time minus the server's time, in ms");
  }

  int execute() override {
    const auto& parameterValues = this->getParameterValues();

    auto factory = pingerFactoryMethods.find(parameterValues.get<std::string>("server"));
    assert(factory != pingerFactoryMethods.cend());

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
