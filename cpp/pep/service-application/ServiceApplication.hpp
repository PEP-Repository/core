#pragma once

#include <pep/application/Application.hpp>
#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/server/NetworkedServer.hpp>
#include <pep/utils/Configuration.hpp> // IWYU pragma: keep (needs the complete type)
#include <pep/utils/Log.hpp> // for Severity

#include <optional>
#include <string>

namespace pep {

class ServiceApplicationBase : public Application {
protected:
  commandline::Parameters getSupportedParameters() const override;
  std::string getDescription() const override;

  std::optional<pep::Severity> consoleLogMinimumSeverityLevel() const override;

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
