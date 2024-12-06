#include <pep/apps/Enrollment.hpp>
#include <pep/apps/Enroller.hpp>

namespace pep {

std::string EnrollmentApplication::getDescription() const {
  return "Enrolls a party with the PEP environment";
}

commandline::Parameters EnrollmentApplication::getSupportedParameters() const {
  return Application::getSupportedParameters()
    + commandline::Parameter("config-file", "Path to configuration file").value(commandline::Value<std::filesystem::path>().positional().required());
}

std::vector<std::shared_ptr<commandline::Command>> EnrollmentApplication::createChildCommands() {
  return {
    std::make_shared<UserEnroller>(*this),
    std::make_shared<ServiceEnroller>(FacilityType::StorageFacility, "Storage Facility", *this),
    std::make_shared<ServiceEnroller>(FacilityType::AccessManager, "Access Manager", *this),
    std::make_shared<ServiceEnroller>(FacilityType::Transcryptor, "Transcryptor", *this),
    std::make_shared<ServiceEnroller>(FacilityType::RegistrationServer, "Registration Server", *this, true)
  };
}

Configuration EnrollmentApplication::getConfiguration() const {
  return Configuration::FromFile(this->getParameterValues().get<std::filesystem::path>("config-file"));
}

}

PEP_DEFINE_MAIN_FUNCTION(pep::EnrollmentApplication)
