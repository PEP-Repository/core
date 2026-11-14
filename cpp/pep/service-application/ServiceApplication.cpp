#include <pep/service-application/ServiceApplication.hpp>
#include <pep/application/Application.hpp>
#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/utils/Log.hpp> // for Severity

#include <optional>
#include <string>

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
