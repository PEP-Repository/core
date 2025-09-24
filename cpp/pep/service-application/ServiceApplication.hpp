#pragma once

#include <pep/application/Application.hpp>
#include <pep/server/NetworkedServer.hpp>

namespace pep {

class ServiceApplicationBase : public Application {
protected:
  commandline::Parameters getSupportedParameters() const override;
  std::string getDescription() const override;

  std::optional<pep::severity_level> consoleLogMinimumSeverityLevel() const override;

  virtual std::string getServiceDescription() const = 0;
};

template <typename TService>
class ServiceApplication : public ServiceApplicationBase {
protected:
  std::string getServiceDescription() const override {
    return GetNormalizedTypeName<TService>();
  }

  int execute() override {
    auto server = NetworkedServer::Make<TService>(this->loadMainConfigFile());
    server.start();
    return 0;
  }
};

}
