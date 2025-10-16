#include <pep/cli/user/CommandUserQuery.hpp>

#include <pep/core-client/CoreClient.hpp>
#include <pep/structuredoutput/Json.hpp>
#include <pep/structuredoutput/Yaml.hpp>
#include <rxcpp/operators/rx-map.hpp>

namespace {

using namespace std::chrono;
using namespace pep::cli;
namespace so = pep::structuredOutput;

}

pep::commandline::Parameters CommandUser::CommandUserQuery::getSupportedParameters() const {
  const auto userGroupsOpt = std::string{so::stringConstants::userGroups.option};
  const auto usersOpt = std::string{so::stringConstants::users.option};
  const auto groupsPerUserOpt = std::string{so::stringConstants::groupsPerUser.option};

  return ChildCommandOf<CommandUser>::getSupportedParameters()
       + pep::commandline::Parameter("script-print", "Prints specified type of data without pretty printing")
             .value(pep::commandline::Value<std::string>().allow(std::vector<std::string>({userGroupsOpt,
                                                                                           usersOpt,
                                                                                           groupsPerUserOpt})))
       + pep::commandline::Parameter("format", "The format of the output.")
             .value(pep::commandline::Value<std::string>()
                        .allow(std::vector<std::string>({"yaml", "json"}))
                        .defaultsTo("yaml", "yaml"))
       + pep::commandline::Parameter("at", "Query for this timestamp (milliseconds since 1970-01-01 00:00:00 in UTC)")
             .value(pep::commandline::Value<milliseconds::rep>().defaultsTo(milliseconds::max().count(), "most recent"))
       + pep::commandline::Parameter("group", "Match these groups")
             .value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"))
       + pep::commandline::Parameter("user", "Match these users")
             .value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"));
}

int CommandUser::CommandUserQuery::execute() {
  return this->executeEventLoopFor([values = this->getParameterValues()](std::shared_ptr<pep::CoreClient> client) {
    return client->userQuery(extractQuery(values)).map([config = extractConfig(values)](pep::UserQueryResponse res) {
      if (config.preferredFormat == so::Format::json) {
        so::json::append(std::cout, res, config) << std::endl;
      }
      else {
        so::yaml::append(std::cout, res, config) << std::endl;
      }
      return pep::FakeVoid();
    });
  });
}

so::DisplayConfig CommandUser::CommandUserQuery::extractConfig(const pep::commandline::NamedValues& values) {
  using Flags = so::DisplayConfig::Flags;
  using Format = so::Format;

  constexpr auto userGroupsOpt = so::stringConstants::userGroups.option;
  constexpr auto usersOpt = so::stringConstants::users.option;
  constexpr auto groupsPerUserOpt = so::stringConstants::groupsPerUser.option;
  const auto scriptPrintFilter = values.getOptional<std::string>("script-print");
  const auto preferredFormat = values.get<std::string>("format");

  so::DisplayConfig config;
  config.flags = Flags::printHeaders * !scriptPrintFilter
               | Flags::printGroups * (!scriptPrintFilter || *scriptPrintFilter == userGroupsOpt)
  //groupsPerUser is a part of the users list. So when groupsPerUsers is requested, we must also print users
               | Flags::printUsers * (!scriptPrintFilter || *scriptPrintFilter == usersOpt || *scriptPrintFilter == groupsPerUserOpt)
               | Flags::printUserGroups * (!scriptPrintFilter || *scriptPrintFilter == groupsPerUserOpt);
  config.preferredFormat = (preferredFormat == "json") ? Format::json : Format::yaml;
  return config;
}

pep::UserQuery CommandUser::CommandUserQuery::extractQuery(const pep::commandline::NamedValues& values) {
  return {
      .mAt = Timestamp(milliseconds{values.get<milliseconds::rep>("at")}),
      .mGroupFilter = values.get<std::string>("group"),
      .mUserFilter = values.get<std::string>("user"),
  };
}
