#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/client/Client.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/messaging/ResponseToVoid.hpp>

#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

namespace {

class CommandPing : public ChildCommandOf<CliApplication> {
private:
  static rxcpp::observable<pep::FakeVoid> PrintCertificateChain(const pep::SigningServerProxy& proxy) {
    return proxy
      .requestCertificateChain()
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

  static rxcpp::observable<pep::FakeVoid> PingAndPrint(const pep::ServerProxy& proxy, bool printDrift) {
    return proxy
      .requestPing()
      .map([printDrift](const pep::PingResponse& response) {
      if (printDrift) {
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

public:
  explicit CommandPing(CliApplication& parent)
    : ChildCommandOf<CliApplication>("ping", "Ping a server", parent) {
  }

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    std::vector<std::string> serverIds;
    auto traits = pep::ServerTraits::All();
    serverIds.reserve(traits.size());
    std::transform(traits.cbegin(), traits.cend(), std::back_inserter(serverIds),
      [](const pep::ServerTraits& single) {return single.commandLineId(); });
    std::sort(serverIds.begin(), serverIds.end());

    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::commandline::Parameter("server", "Server to ping").value(pep::commandline::Value<std::string>().required().allow(serverIds))
      + pep::commandline::Parameter("print-certificate-chain", "Print the server's certificate chain")
      + pep::commandline::Parameter("print-drift", "Print local time minus the server's time, in ms");
  }

  int execute() override {
    const auto& parameterValues = this->getParameterValues();

    auto printCertificateChain = parameterValues.has("print-certificate-chain");
    auto printDrift = parameterValues.has("print-drift");
    if (printDrift && printCertificateChain) {
      LOG(LOG_TAG, pep::error) << "--print-drift and --print-certificate-chain"
        << " can not be combined.";
      return 3;
    }

    auto serverId = parameterValues.get<std::string>("server");
    auto traits = pep::ServerTraits::Find([serverId](const pep::ServerTraits& candidate) {
      return candidate.commandLineId() == serverId;
      });
    assert(traits.has_value());

    if (printCertificateChain && traits->userGroups().empty()) {
      LOG(LOG_TAG, pep::error) << traits->description() << " does not produce a certificate chain to print";
      return 3;
    }

    return this->executeEventLoopFor(false, [traits = *traits, printCertificateChain, printDrift](std::shared_ptr<pep::Client> client) {
      auto proxy = client->getServerProxy(traits);
      if (printCertificateChain) {
        return PrintCertificateChain(static_cast<const pep::SigningServerProxy&>(*proxy)); // TODO: prevent downcast
      }

      return PingAndPrint(*proxy, printDrift);
      });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandPing(CliApplication& parent) {
  return std::make_shared<CommandPing>(parent);
}
