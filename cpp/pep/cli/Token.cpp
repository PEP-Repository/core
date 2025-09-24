#include <pep/cli/Token.hpp>
#include <pep/cli/token/CommandTokenBlock.hpp>
#include <pep/cli/Commands.hpp>

#include <pep/client/Client.hpp>
#include <pep/crypto/Timestamp.hpp>
#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandToken(CliApplication& parent) {
  return std::make_shared<CommandToken>(parent);
}

class CommandToken::RequestTokenCommand : public ChildCommandOf<CommandToken> {
public:
  explicit RequestTokenCommand(CommandToken& parent)
    : ChildCommandOf<CommandToken>("request", "Request an oauth token", parent) {}

private:
  static std::string GetRequiredExpirationSpecificationMessage() {
    return "Please specify either an --expiration-yyyymmdd switch or an [expiration-unixtime] parameter, but not both.";
  }

protected:
  std::optional<std::string> getAdditionalDescription() const override {
    return GetRequiredExpirationSpecificationMessage();
  }

  void finalizeParameters() override {
    ChildCommandOf<CommandToken>::finalizeParameters();

    const auto& values = this->getParameterValues();
    if (values.has("expiration-unixtime") == values.has("expiration-yyyymmdd")) {
      throw std::runtime_error(GetRequiredExpirationSpecificationMessage());
    }
  }

  int execute() override {
    const auto& values = this->getParameterValues();

    auto expiration = values.has("expiration-unixtime")
        ? static_cast<time_t>(values.get<int64_t>("expiration-unixtime"))
        : pep::Timestamp::FromIsoDate(values.get<std::string>("expiration-yyyymmdd")).toTime_t();

    return executeEventLoopFor([&values, expiration](std::shared_ptr<pep::Client> client) {
      return client
          ->requestToken(
              values.get<std::string>("subject"),
              values.get<std::string>("user-group"),
              pep::Timestamp::FromTimeT(expiration))
          .map([json = values.has("json")](const std::string& token) {
            const auto decorate = [](const std::string& s) { return "{\n  \"OAuthToken\": \"" + s + "\"\n}"; };
            std::cout << (json ? decorate(token) : token) << std::endl;
            return pep::FakeVoid();
          });
    });
  }

  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandToken>::getSupportedParameters()
        + pep::commandline::Parameter("subject", "The subject (user) to request a token for")
              .value(pep::commandline::Value<std::string>().positional().required())
        + pep::commandline::Parameter("user-group", "The user group to request a token for")
              .value(pep::commandline::Value<std::string>().positional().required())
        + pep::commandline::Parameter(
              "expiration-unixtime",
              "The expiration time for the token, expressed as a Unix epoch")
              .value(pep::commandline::Value<int64_t>().positional())
        + pep::commandline::Parameter("expiration-yyyymmdd", "The expiration time for the token, expressed as a date")
              .value(pep::commandline::Value<std::string>())
        + pep::commandline::Parameter("json", "Produce output in JSON format");
  }
};

std::vector<std::shared_ptr<pep::commandline::Command>> CommandToken::createChildCommands() {
  return {std::make_shared<RequestTokenCommand>(*this), std::make_shared<CommandTokenBlock>(*this)};
}
