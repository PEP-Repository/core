#include <pep/cli/asa/CommandAsaToken.hpp>
#include <pep/cli/asa/CommandAsaTokenBlock.hpp>

#include <pep/crypto/Timestamp.hpp>
#include <rxcpp/operators/rx-map.hpp>

class CommandAsa::CommandAsaToken::RequestTokenCommand : public ChildCommandOf<CommandAsa::CommandAsaToken> {
public:
  explicit RequestTokenCommand(CommandAsaToken& parent)
    : ChildCommandOf<CommandAsaToken>("request", "Request an oauth token", parent) {}

private:
  static std::string GetRequiredExpirationSpecificationMessage() {
    return "Please specify either an --expiration-yyyymmdd switch or an [expiration-unixtime] parameter, but not both.";
  }

protected:
  std::optional<std::string> getAdditionalDescription() const override {
    return GetRequiredExpirationSpecificationMessage();
  }

  void finalizeParameters() override {
    ChildCommandOf<CommandAsaToken>::finalizeParameters();

    const auto& values = this->getParameterValues();
    if (values.has("expiration-unixtime") == values.has("expiration-yyyymmdd")) {
      throw std::runtime_error(GetRequiredExpirationSpecificationMessage());
    }
  }

  int execute() override {
    const auto& values = this->getParameterValues();

    const int64_t expiration = values.has("expiration-unixtime")
        ? values.get<int64_t>("expiration-unixtime")
        : pep::Timestamp::FromIsoDate(values.get<std::string>("expiration-yyyymmdd")).getTime();

    return executeEventLoopFor([&values, expiration](std::shared_ptr<pep::Client> client) {
      return client
          ->asaRequestToken(
              values.get<std::string>("subject"),
              values.get<std::string>("user-group"),
              pep::Timestamp(expiration))
          .map([json = values.has("json")](const std::string& token) {
            const auto decorate = [](const std::string& s) { return "{\n  \"OAuthToken\": \"" + s + "\"\n}"; };
            std::cout << (json ? decorate(token) : token) << std::endl;
            return pep::FakeVoid();
          });
    });
  }

  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandAsaToken>::getSupportedParameters()
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

std::vector<std::shared_ptr<pep::commandline::Command>> CommandAsa::CommandAsaToken::createChildCommands() {
  return {std::make_shared<RequestTokenCommand>(*this), std::make_shared<CommandAsaTokenBlock>(*this)};
}