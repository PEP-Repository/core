#include <pep/application/Application.hpp>
#include <pep/servers/Servers.hpp>
#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/utils/Log.hpp> // for Severity

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
    auto configPath = this->getConfigDirectory();

    Servers servers;
    servers.runAsync(configPath);
    servers.wait();

    return -1;
  }
};
}

}

PEP_DEFINE_MAIN_FUNCTION(pep::ServersApplication)
