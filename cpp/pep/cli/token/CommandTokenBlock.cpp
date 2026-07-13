#include <pep/cli/token/CommandTokenBlock.hpp>

#include <pep/client/Client.hpp>
#include <pep/crypto/Timestamp.hpp>
#include <pep/structuredoutput/Csv.hpp>
#include <pep/structuredoutput/Table.hpp>

using namespace pep::cli;

namespace {

std::ostream& appendTable(std::ostream& stream, const std::vector<pep::tokenBlocking::BlocklistEntry>& entries) {
  auto table = pep::structuredOutput::Table::EmptyWithHeader(
      {"id", "targetSubject", "targetUserGroup", "targetIssueDateTime", "note", "issuer", "creationDateTime"});
  table.reserve(entries.size());

  for (const auto& e : entries) {
    table.emplace_back(
        {std::to_string(e.id),
         e.target.subject,
         e.target.userGroup,
         pep::TimestampToXmlDateTime(e.target.issueDateTime),
         e.metadata.note,
         e.metadata.issuer,
         pep::TimestampToXmlDateTime(e.metadata.creationDateTime)});
  }

  return pep::structuredOutput::csv::append(stream, table) << std::endl;
}

namespace cliParameterNames {

constexpr auto subject = "subject";
constexpr auto userGroup = "user-group";
constexpr auto issuedBefore = "issuedBefore";
constexpr auto message = "message";

} // namespace cliParameterNames

} // namespace

class CommandToken::CommandTokenBlock::SubcommandCreate
  : public ChildCommandOf<CommandTokenBlock> {
public:
  explicit SubcommandCreate(CommandTokenBlock& parent)
    : ChildCommandOf<CommandTokenBlock>("create", "Block additional tokens by adding a new blocking rule.", parent) {
  }

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    namespace param = cliParameterNames;
    return ChildCommandOf<CommandTokenBlock>::getSupportedParameters()
        + pep::commandline::Parameter(
              param::subject,
              "only block tokens that were issued for the specified interactive user")
              .value(pep::commandline::Value<std::string>().positional().required())
        + pep::commandline::Parameter(
              param::userGroup,
              "only block tokens that were issued for the specified user-group")
              .value(pep::commandline::Value<std::string>().positional().required())
        + pep::commandline::Parameter(
              param::issuedBefore,
              "only block tokens that were issued before the specified date")
              .alias("before")
              .shorthand('b')
              .value(pep::commandline::Value<Timestamp>())
        + pep::commandline::Parameter(
              param::message,
              "explanatory text stored together with the created blocklist entry")
              .shorthand('m')
              .value(pep::commandline::Value<std::string>().required());
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
              .issueDateTime = values.getOptional<Timestamp>(param::issuedBefore).value_or(TimeNow())
          },
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
        return client->getKeyServerProxy()->requestTokenBlockingCreate(TokenBlockingCreateRequest{ config.target, config.message }).map(printResponse);
        });
  }
};

class CommandToken::CommandTokenBlock::SubcommandRemove
  : public ChildCommandOf<CommandTokenBlock> {
public:
  explicit SubcommandRemove(CommandTokenBlock& parent)
    : ChildCommandOf<CommandTokenBlock>("remove", "Remove an existing blocking rule.", parent) {}

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandTokenBlock>::getSupportedParameters()
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
        return client->getKeyServerProxy()->requestTokenBlockingRemove(TokenBlockingRemoveRequest{ config.entryId }).map(printResponse);
        });
  }
};

class CommandToken::CommandTokenBlock::SubcommandList : public ChildCommandOf<CommandTokenBlock> {
public:
  explicit SubcommandList(CommandTokenBlock& parent)
    : ChildCommandOf<CommandTokenBlock>("list", "List all active token blocking rules.", parent) {}

protected:
  int execute() override {
    const auto printResponse = [](const pep::TokenBlockingListResponse& response) {
      appendTable(std::cout, response.entries);
      return pep::FakeVoid{};
    };

    return executeEventLoopFor(
        [&](std::shared_ptr<pep::Client> client) { return client->getKeyServerProxy()->requestTokenBlockingList().map(printResponse); });
  }
};

std::vector<std::shared_ptr<pep::commandline::Command>> CommandToken::CommandTokenBlock::
    createChildCommands() {
  return {
      std::make_shared<SubcommandCreate>(*this),
      std::make_shared<SubcommandRemove>(*this),
      std::make_shared<SubcommandList>(*this)};
}
