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
      + commandline::Parameter("config-dir", "Top level configuration directory").value(commandline::Value<std::filesystem::path>()
        .directory().positional().defaultsTo("."));
  }

  int execute() override {
    std::filesystem::path configPath(std::filesystem::absolute(this->getParameterValues().get<std::filesystem::path>("config-dir")));
    this->tryLoadConfigVersion(configPath);

    Servers servers;
    servers.runAsync(configPath);
    servers.wait();

    return -1;
  }
};

}

PEP_DEFINE_MAIN_FUNCTION(pep::ServersApplication)
