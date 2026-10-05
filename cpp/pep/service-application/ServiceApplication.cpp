#include <pep/service-application/ServiceApplication.hpp>

namespace pep {

commandline::Parameters ServiceApplicationBase::getSupportedParameters() const {
  return Application::getSupportedParameters()
    + MakeConfigFileParameters(".", std::nullopt, true);
}

std::string ServiceApplicationBase::getDescription() const {
  return "Service application for " + this->getServiceDescription();
}

std::optional<pep::Severity> ServiceApplicationBase::consoleLogMinimumSeverityLevel() const {
  return Severity::Info;
}

}
