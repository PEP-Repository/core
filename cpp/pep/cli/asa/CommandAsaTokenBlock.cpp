#include <pep/cli/asa/CommandAsaTokenBlock.hpp>

#include <pep/crypto/Timestamp.hpp>

namespace {

std::ostream& appendTable(std::ostream& stream, const std::vector<pep::tokenBlocking::BlocklistEntry>& entries) {
  stream << "id, targetSubject, targetUserGroup, targetIssueDateTime, note, issuer, creationDateTime\n";
  for (const auto& e : entries) {
    stream << e.id << ", " << e.target.subject << ", " << e.target.userGroup << ", "
           << e.target.issueDateTime.toString() << ", " << e.metadata.note << ", " << e.metadata.issuer << ", "
           << e.metadata.creationDateTime.toString() << "\n";
  }
  return stream << std::endl;
}

namespace cliParameterNames {

constexpr auto subject = "subject";
constexpr auto userGroup = "user-group";
constexpr auto issuedBeforeUnixtime = "issuedBefore-unixtime";
constexpr auto issuedBeforeYyyymmdd = "issuedBefore-yyyymmdd";
constexpr auto message = "message";

} // namespace cliParameterNames

} // namespace

class CommandAsa::CommandAsaToken::CommandAsaTokenBlock::SubcommandCreate
  : public ChildCommandOf<CommandAsaTokenBlock> {
public:
  explicit SubcommandCreate(CommandAsaTokenBlock& parent)
    : ChildCommandOf<CommandAsaTokenBlock>("create", "Block additional tokens by adding a new blocking rule.", parent) {
  }

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    namespace param = cliParameterNames;
    return ChildCommandOf<CommandAsaTokenBlock>::getSupportedParameters()
        + pep::commandline::Parameter(
              param::subject,
              "only block tokens that were issued for the specified interactive user")
              .value(pep::commandline::Value<std::string>().positional().required())
        + pep::commandline::Parameter(
              param::userGroup,
              "only block tokens that were issued for the specified user-group")
              .value(pep::commandline::Value<std::string>().positional().required())
        + pep::commandline::Parameter(
              param::issuedBeforeUnixtime,
              "only block tokens that were issued before the specified unix timestamp")
              .value(pep::commandline::Value<int64_t>())
        + pep::commandline::Parameter(
              param::issuedBeforeYyyymmdd,
              "only block tokens that were issued before the specified date")
              .alias("before")
              .shorthand('b')
              .value(pep::commandline::Value<std::string>())
        + pep::commandline::Parameter(
              param::message,
              "explanatory text stored together with the created blocklist entry")
              .shorthand('m')
              .value(pep::commandline::Value<std::string>().required());
  }

  virtual void finalizeParameters() override {
    ChildCommandOf<CommandAsaTokenBlock>::finalizeParameters();

    const auto& values = this->getParameterValues();
    namespace param = cliParameterNames;
    if (values.has(param::issuedBeforeUnixtime) && values.has(param::issuedBeforeYyyymmdd)) {
      throw std::runtime_error(
          "Please specify the target issued date/time either via the --" + std::string{param::issuedBeforeYyyymmdd}
          + " switch or the [" + std::string{param::issuedBeforeUnixtime} + "] parameter, but not both.");
    }
  }

  struct Configuration final {
    pep::tokenBlocking::TokenIdentifier target;
    std::string message;

    static Configuration From(const pep::commandline::NamedValues& values) {
      namespace param = cliParameterNames;
      return {
          .target{
              .subject = values.get<std::string>(param::subject),
              .userGroup = values.get<std::string>(param::userGroup),
              .issueDateTime =
                  [&] {
                    if (const auto date = values.getOptional<std::string>(param::issuedBeforeYyyymmdd)) {
                      return pep::Timestamp::FromIsoDate(*date);
                    }
                    if (const auto time = values.getOptional<int64_t>(param::issuedBeforeUnixtime)) {
                      return pep::Timestamp::FromTimeT(*time);
                    }
                    return pep::Timestamp{}; // default to current time
                  }()},
          .message = values.get<std::string>(param::message)};
    }
  };

  int execute() override {
    using namespace rxcpp::operators;
    const auto printResponse = [](const pep::TokenBlockingCreateResponse& response) {
      appendTable(std::cout, {response.entry});
      return pep::FakeVoid{};
    };

    return executeEventLoopFor(
        [&, config = Configuration::From(this->getParameterValues())](std::shared_ptr<pep::Client> client) {
          return client->tokenBlockCreate(std::move(config.target), std::move(config.message)).map(printResponse);
        });
  }
};

class CommandAsa::CommandAsaToken::CommandAsaTokenBlock::SubcommandRemove
  : public ChildCommandOf<CommandAsaTokenBlock> {
public:
  explicit SubcommandRemove(CommandAsaTokenBlock& parent)
    : ChildCommandOf<CommandAsaTokenBlock>("remove", "Remove an existing blocking rule.", parent) {}

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandAsaTokenBlock>::getSupportedParameters()
        + pep::commandline::Parameter(
              "id",
              "the numeric id of the entry to be removed, as shown in sister command 'list'")
              .value(pep::commandline::Value<int64_t>().positional().required());
  }

  struct Configuration final {
    int64_t entryId;

    static Configuration From(const pep::commandline::NamedValues& values) {
      return {.entryId = values.get<int64_t>("id")};
    }
  };

  int execute() override {
    const auto printResponse = [](const pep::TokenBlockingRemoveResponse& response) {
      appendTable(std::cout, {response.entry});
      return pep::FakeVoid{};
    };

    return executeEventLoopFor(
        [&, config = Configuration::From(this->getParameterValues())](std::shared_ptr<pep::Client> client) {
          return client->tokenBlockRemove(config.entryId).map(printResponse);
        });
  }
};

class CommandAsa::CommandAsaToken::CommandAsaTokenBlock::SubcommandList : public ChildCommandOf<CommandAsaTokenBlock> {
public:
  explicit SubcommandList(CommandAsaTokenBlock& parent)
    : ChildCommandOf<CommandAsaTokenBlock>("list", "List all active token blocking rules.", parent) {}

protected:
  int execute() override {
    const auto printResponse = [](const pep::TokenBlockingListResponse& response) {
      appendTable(std::cout, response.entries);
      return pep::FakeVoid{};
    };

    return executeEventLoopFor(
        [&](std::shared_ptr<pep::Client> client) { return client->tokenBlockList().map(printResponse); });
  }
};

std::vector<std::shared_ptr<pep::commandline::Command>> CommandAsa::CommandAsaToken::CommandAsaTokenBlock::
    createChildCommands() {
  return {
      std::make_shared<SubcommandCreate>(*this),
      std::make_shared<SubcommandRemove>(*this),
      std::make_shared<SubcommandList>(*this)};
}
