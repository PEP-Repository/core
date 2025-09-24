#include <pep/application/Application.hpp>
#include <pep/utils/Win32Api.hpp>

#include <boost/algorithm/string/join.hpp>

namespace {

class PepElevateApplication : public pep::Application {
private:
  constexpr std::nullopt_t suppressLogging() const noexcept {
    // Boost's logging code has been observed to raise exceptions
    return std::nullopt;
  }

protected:
  std::string getDescription() const override {
    return "Runs an executable in an elevated context";
  }

  virtual pep::commandline::Parameters getSupportedParameters() const {
    return pep::Application::getSupportedParameters()
      + pep::commandline::Parameter("executable", "Executable to run").value(pep::commandline::Value<std::filesystem::path>().positional().required())
      + pep::commandline::Parameter("parameter", "Parameters to pass to the executable").value(pep::commandline::Value<std::string>().positional().multiple());
  }

  std::optional<pep::severity_level> syslogLogMinimumSeverityLevel() const override {
    return suppressLogging();
  }
  std::optional<pep::severity_level> consoleLogMinimumSeverityLevel() const override {
    return suppressLogging();
  }
  std::optional<pep::severity_level> fileLogMinimumSeverityLevel() const override {
    return suppressLogging();
  }

  int execute() override {
    const auto& values = this->getParameterValues();

    auto executable = values.get<std::filesystem::path>("executable");
    auto parameters = values.getOptionalMultiple<std::string>("parameter");

    pep::win32api::StartProcess(executable, boost::algorithm::join(parameters, " "), true);

    return EXIT_SUCCESS;
  }
};

}

PEP_DEFINE_MAIN_FUNCTION(PepElevateApplication)
