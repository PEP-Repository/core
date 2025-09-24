#include <pep/application/Application.hpp>
#include <pep/pullcastor/EnvironmentPuller.hpp>

namespace {

class CastorPullApplication : public pep::Application {
protected:
  std::optional<pep::severity_level> consoleLogMinimumSeverityLevel() const override {
    return pep::severity_level::info;
  }

  std::optional<pep::severity_level> fileLogMinimumSeverityLevel() const override {
    return pep::severity_level::debug;
  }

  std::string getDescription() const override {
    return "Imports Castor data into PEP";
  }

  pep::commandline::Parameters getSupportedParameters() const override {
    return pep::Application::getSupportedParameters()
      + pep::commandline::Parameter("dry", "Perform a dry run: don't store")
      + pep::commandline::Parameter("sp-column", "Process only the specified short pseudonym column(s)").value(pep::commandline::Value<std::string>().multiple())
      + pep::commandline::Parameter("sp", "Process only the specified short pseudonym(s), i.e. Castor participant ID(s)").value(pep::commandline::Value<std::string>().multiple())
      + MakeConfigFileParameters(".", std::nullopt, true);
  }

  int execute() override {
    const auto& values = this->getParameterValues();
    auto config = this->loadMainConfigFile();
    std::optional<std::vector<std::string>> spColumns, sps;
    if (values.has("sp-column")) {
      spColumns = values.getMultiple<std::string>("sp-column");
    }
    if (values.has("sp")) {
      sps = values.getMultiple<std::string>("sp");
    }
    return pep::castor::EnvironmentPuller::Pull(config, values.has("dry"), spColumns, sps)
      ? EXIT_SUCCESS
      : EXIT_FAILURE;
  }
};

}

PEP_DEFINE_MAIN_FUNCTION(CastorPullApplication)
