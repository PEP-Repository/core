#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/application/Application.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/client/Client.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

namespace {

class CommandMetrics : public ChildCommandOf<CliApplication> {
public:
  explicit CommandMetrics(CliApplication& parent)
    : ChildCommandOf<CliApplication>("metrics", "Retrieves metrics", parent) {
  }

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CliApplication>::getSupportedParameters()
      + pep::commandline::Parameter("server", "Restrict to specified server(s)").value(pep::commandline::Value<std::string>().positional().multiple()
        .allow(std::vector<std::string>({"accessmanager", "authserver", "keyserver", "registrationserver", "transcryptor", "storagefacility" }))); // TODO: don't require explicit std::vector<std::string>(...) instantiation
  }

  using ServerFilter = std::unordered_set<std::string>;
  using GetServerProxy = std::function<std::shared_ptr<const pep::ServerProxy>()>;

  static rxcpp::observable<std::string> GetServerMetrics(const ServerFilter& filter, const std::string& name, const std::string& caption, const GetServerProxy& getServerProxy) {
    if (filter.empty() || filter.contains(name)) {
      return getServerProxy()->requestMetrics().map([caption](pep::MetricsResponse metrics) {
        std::ostringstream oss;
        oss << "============================ " << caption << " ============================\n";
        oss << metrics.mMetrics;
        return std::move(oss).str();
        });
    }
    return rxcpp::observable<>::empty<std::string>();
  }

  int execute() override {
    std::vector<std::string> serverFilterVec = this->getParameterValues().getOptionalMultiple<std::string>("server");
    ServerFilter serverFilter(serverFilterVec.begin(), serverFilterVec.end());

    return this->executeEventLoopFor([serverFilter](std::shared_ptr<pep::Client> client) {
      std::vector<rxcpp::observable<std::string>> observables;
      observables.push_back(GetServerMetrics(serverFilter, "accessmanager",       "Access Manager",       [client]() {return client->getAccessManagerProxy(); }));
      observables.push_back(GetServerMetrics(serverFilter, "authserver",          "AuthServer",           [client]() {return client->getAuthServerProxy(); }));
      observables.push_back(GetServerMetrics(serverFilter, "keyserver",           "KeyServer",            [client]() {return client->getKeyServerProxy(); }));
      observables.push_back(GetServerMetrics(serverFilter, "registrationserver",  "Registration Server",  [client]() {return client->getRegistrationServerProxy(); }));
      observables.push_back(GetServerMetrics(serverFilter, "transcryptor",        "Transcryptor",         [client]() {return client->getTranscryptorProxy(); }));
      observables.push_back(GetServerMetrics(serverFilter, "storagefacility",     "Storage Facility",     [client]() {return client->getStorageFacilityProxy(); }));

      return rxcpp::rxs::iterate(observables).concat().map([](std::string metricsString){
        std::cout << metricsString << std::endl;
        return pep::FakeVoid();
      });
    });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandMetrics(CliApplication& parent) {
  return std::make_shared<CommandMetrics>(parent);
}
