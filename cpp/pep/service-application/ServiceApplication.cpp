#include <pep/service-application/ServiceApplication.hpp>

#include <iostream>

namespace pep {

commandline::Parameters ServiceApplicationBase::getSupportedParameters() const {
  return Application::getSupportedParameters()
    + commandline::Parameter("config", "Path to configuration file").value(commandline::Value<std::filesystem::path>().positional().required());
}

std::string ServiceApplicationBase::getDescription() const {
  return "Service application for " + this->getServiceDescription();
}

int ServiceApplicationBase::execute() {
  auto configFile = this->getParameterValues().get<std::filesystem::path>("config");
  this->tryLoadConfigVersion(std::filesystem::path(configFile).parent_path());
  this->runService(configFile.string().data());
  return 0;
}

std::optional<pep::severity_level> ServiceApplicationBase::consoleLogMinimumSeverityLevel() const {
  return pep::severity_level::info;
}

}
