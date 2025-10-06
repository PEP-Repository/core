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

  int execute() override {
    std::vector<std::string> serverFilterVec = this->getParameterValues().getOptionalMultiple<std::string>("server");
    std::unordered_set<std::string> serverFilter(serverFilterVec.begin(), serverFilterVec.end());

    return this->executeEventLoopFor([serverFilter](std::shared_ptr<pep::Client> client) {
      std::vector<rxcpp::observable<std::string>> observables;
      if(serverFilter.empty() || serverFilter.find("accessmanager") != serverFilter.end()) {
        observables.push_back(client->getAccessManagerMetrics().map([](pep::MetricsResponse metrics) {
          std::ostringstream oss;
          oss << "============================ Access Manager ============================\n";
          oss << metrics.mMetrics;
          return oss.str();
        }));
      }
      if(serverFilter.empty() || serverFilter.find("authserver") != serverFilter.end()) {
        observables.push_back(client->getAuthserverMetrics().map([](pep::MetricsResponse metrics) {
          std::ostringstream oss;
          oss << "============================ AuthServer ============================\n";
          oss << metrics.mMetrics;
          return oss.str();
        }));
      }
      if(serverFilter.empty() || serverFilter.find("keyserver") != serverFilter.end()) {
        observables.push_back(client->getKeyClient()->requestMetrics().map([](pep::MetricsResponse metrics) {
          std::ostringstream oss;
          oss << "============================ KeyServer ============================\n";
          oss << metrics.mMetrics;
          return oss.str();
        }));
      }
      if(serverFilter.empty() || serverFilter.find("registrationserver") != serverFilter.end()) {
        observables.push_back(client->getRegistrationServerMetrics().map([](pep::MetricsResponse metrics){
          std::ostringstream oss;
          oss << "============================ Registration Server ============================\n";
          oss << metrics.mMetrics;
          return oss.str();
        }));
      }
      if(serverFilter.empty() || serverFilter.find("transcryptor") != serverFilter.end()) {
        observables.push_back(client->getTranscryptorMetrics().map([](pep::MetricsResponse metrics){
          std::ostringstream oss;
          oss << "============================ Transcryptor ============================\n";
          oss << metrics.mMetrics;
          return oss.str();
        }));
      }
      if(serverFilter.empty() || serverFilter.find("storagefacility") != serverFilter.end()) {
        observables.push_back(client->getStorageClient()->requestMetrics().map([](pep::MetricsResponse metrics) {
          std::ostringstream oss;
          oss << "============================ Storage Facility ============================\n";
          oss << metrics.mMetrics;
          return oss.str();
        }));
      }
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
