#include <pep/application/Application.hpp>
#include <pep/servers/Servers.hpp>


namespace pep {

class ServersApplication : public Application {
protected:
  std::optional<pep::severity_level> consoleLogMinimumSeverityLevel() const override {
    return pep::severity_level::info;
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

PEP_DEFINE_MAIN_FUNCTION(pep::ServersApplication)
