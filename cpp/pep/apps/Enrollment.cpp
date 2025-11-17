#include <pep/apps/Enrollment.hpp>
#include <pep/apps/Enroller.hpp>

namespace pep {

std::string EnrollmentApplication::getDescription() const {
  return "Enrolls a party with the PEP environment";
}

commandline::Parameters EnrollmentApplication::getSupportedParameters() const {
  return Application::getSupportedParameters()
    + MakeConfigFileParameters(".", std::nullopt, true);
}

std::vector<std::shared_ptr<commandline::Command>> EnrollmentApplication::createChildCommands() {
  auto servers = ServerTraits::Where([](const ServerTraits& server) { return server.isEnrollable(); });
  // Sort by EnrolledParty value: produces nicely sorted child commands
  std::sort(servers.begin(), servers.end(), [](const ServerTraits& lhs, const ServerTraits& rhs) {
    assert(lhs.enrollsAs().has_value());
    assert(rhs.enrollsAs().has_value());
    return *lhs.enrollsAs() < *rhs.enrollsAs();
    });

  std::vector<std::shared_ptr<commandline::Command>> result;
  result.reserve(servers.size() + 1U);
  result.emplace_back(std::make_shared<UserEnroller>(*this));

  for (auto& server : servers) {
    result.emplace_back(std::make_shared<ServiceEnroller>(std::move(server), *this));
  }

  return result;
}

Configuration EnrollmentApplication::getConfiguration() {
  return this->loadMainConfigFile();
}

}

PEP_DEFINE_MAIN_FUNCTION(pep::EnrollmentApplication)
