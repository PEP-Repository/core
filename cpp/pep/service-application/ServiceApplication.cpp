#include <pep/service-application/ServiceApplication.hpp>

namespace pep {

commandline::Parameters ServiceApplicationBase::getSupportedParameters() const {
  return Application::getSupportedParameters()
    + MakeConfigFileParameters(".", std::nullopt, true);
}

std::string ServiceApplicationBase::getDescription() const {
  return "Service application for " + this->getServiceDescription();
}

std::optional<pep::severity_level> ServiceApplicationBase::consoleLogMinimumSeverityLevel() const {
  return pep::severity_level::info;
}

}
