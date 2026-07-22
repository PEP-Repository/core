#include <pep/cli/Token.hpp>
#include <pep/cli/token/CommandTokenBlock.hpp>
#include <pep/cli/Commands.hpp>

#include <pep/client/Client.hpp>
#include <pep/utils/Timestamp.hpp>
#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandToken(CliApplication& parent) {
  return std::make_shared<CommandToken>(parent);
}

class CommandToken::RequestTokenCommand : public ChildCommandOf<CommandToken> {
public:
  explicit RequestTokenCommand(CommandToken& parent)
    : ChildCommandOf<CommandToken>("request", "Request an oauth token", parent) {}

protected:
  int execute() override {
    const auto& values = this->getParameterValues();

    return executeEventLoopFor([&values](std::shared_ptr<pep::Client> client) {
      return client
          ->getAuthServerProxy()
          ->requestToken(
              values.get<std::string>("subject"),
              values.get<std::string>("user-group"),
              values.get<Timestamp>("expiration"))
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
              "expiration",
              "The expiration time for the token")
              .value(pep::commandline::Value<Timestamp>().positional())
        + pep::commandline::Parameter("json", "Produce output in JSON format");
  }
};

std::vector<std::shared_ptr<pep::commandline::Command>> CommandToken::createChildCommands() {
  return {std::make_shared<RequestTokenCommand>(*this), std::make_shared<CommandTokenBlock>(*this)};
}
