#include <pep/application/Application.hpp>
#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/servers/Servers.hpp>
#include <pep/service-application/TerminationSignalHandler.hpp>
#include <pep/utils/Log.hpp> // for Severity

#include <cstdlib>
#include <optional>
#include <string>


namespace pep {

namespace {
class ServersApplication : public Application {
protected:
  std::optional<Severity> consoleLogMinimumSeverityLevel() const override {
    return Severity::Info;
  }

  std::string getDescription() const override {
    return "Runs all PEP services in a single process";
  }

  commandline::Parameters getSupportedParameters() const override {
    return Application::getSupportedParameters()
      + MakeConfigDirectoryParameter(".", true);
  }

  int execute() override {
    Servers servers(this->getConfigDirectory());
    TerminationSignalHandler stopOnTermination([&servers] { servers.stop(); });
    servers.run();
    return EXIT_SUCCESS;
  }
};
}

}

PEP_DEFINE_MAIN_FUNCTION(pep::ServersApplication)
