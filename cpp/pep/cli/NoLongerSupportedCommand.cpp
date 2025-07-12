#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>

using namespace pep::cli;

class NoLongerSupportedCommand : public ChildCommandOf<CliApplication> {
public:
  explicit NoLongerSupportedCommand(CliApplication& parent, const std::string& name, const std::string& message)
    : ChildCommandOf<CliApplication>(name, "No longer supported. " + message, parent), mMessage(message) {}

  bool isUndocumented() const override { return true; }

  pep::commandline::Parameters getSupportedParameters() const override {
    // We don't want to print messages about unrecognized parameters. We just want to print a message that this command is no longer supported. So we eat all parameters.
    return ChildCommandOf<CliApplication>::getSupportedParameters() + pep::commandline::Parameter("ignored", std::nullopt).value(pep::commandline::Value<std::string>().eatAll());
  }

  int execute() override {
    std::cerr << "The command '" << this->getName() << " ' is no longer supported. " << mMessage << std::endl;
    return EXIT_FAILURE;
  }

private:
  std::string mMessage;
};

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateNoLongerSupportedCommand(CliApplication& parent, const std::string& name, const std::string& description) {
  return std::make_shared<NoLongerSupportedCommand>(parent, name, description);
}

