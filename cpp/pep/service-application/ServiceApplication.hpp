#pragma once

#include <pep/application/Application.hpp>
#include <pep/service-application/Service.hpp>

namespace pep {

class ServiceApplicationBase : public Application {
protected:
  commandline::Parameters getSupportedParameters() const override;
  std::string getDescription() const override;
  int execute() override;

  std::optional<pep::severity_level> consoleLogMinimumSeverityLevel() const override;

  virtual std::string getServiceDescription() const = 0;
  virtual void runService(const char *configurationFile) const = 0;
};

template <typename TServer>
class ServiceApplication : public ServiceApplicationBase {
protected:
  std::string getServiceDescription() const override {
    return GetNormalizedTypeName<TServer>();
  }

  void runService(const char *configurationFile) const override {
    Service<TServer>(configurationFile).run();
  }
};

}
