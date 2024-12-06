#include <pep/cli/asa/CommandAsaQuery.hpp>

#include <pep/cli/structuredoutput/Json.hpp>
#include <pep/cli/structuredoutput/Yaml.hpp>
#include <rxcpp/operators/rx-map.hpp>

pep::commandline::Parameters CommandAsa::CommandAsaQuery::getSupportedParameters() const {
  const auto userGroupsOpt = std::string{structuredOutput::stringConstants::userGroups.option};
  const auto usersOpt = std::string{structuredOutput::stringConstants::users.option};
  const auto groupsPerUserOpt = std::string{structuredOutput::stringConstants::groupsPerUser.option};

  return ChildCommandOf<CommandAsa>::getSupportedParameters()
       + pep::commandline::Parameter("script-print", "Prints specified type of data without pretty printing")
             .value(pep::commandline::Value<std::string>().allow(std::vector<std::string>({userGroupsOpt,
                                                                                           usersOpt,
                                                                                           groupsPerUserOpt})))
       + pep::commandline::Parameter("format", "The format of the output.")
             .value(pep::commandline::Value<std::string>()
                        .allow(std::vector<std::string>({"yaml", "json"}))
                        .defaultsTo("yaml", "yaml"))
       + pep::commandline::Parameter("at", "Query for this timestamp (milliseconds since 1970-01-01 00:00:00 in UTC)")
             .value(pep::commandline::Value<int64_t>().defaultsTo(pep::Timestamp::max().getTime(), "most recent"))
       + pep::commandline::Parameter("group", "Match these groups")
             .value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"))
       + pep::commandline::Parameter("user", "Match these users")
             .value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"));
}

int CommandAsa::CommandAsaQuery::execute() {
  return this->executeEventLoopFor([values = this->getParameterValues()](std::shared_ptr<pep::Client> client) {
    return client->asaQuery(extractQuery(values)).map([config = extractConfig(values)](pep::AsaQueryResponse res) {
      if (config.preferredFormat == structuredOutput::Format::json) {
        structuredOutput::json::append(std::cout, res, config) << std::endl;
      }
      else {
        structuredOutput::yaml::append(std::cout, res, config) << std::endl;
      }
      return pep::FakeVoid();
    });
  });
}

structuredOutput::DisplayConfig CommandAsa::CommandAsaQuery::extractConfig(const pep::commandline::NamedValues&
                                                                               values) {
  using Flags = structuredOutput::DisplayConfig::Flags;
  using Format = structuredOutput::Format;

  constexpr auto userGroupsOpt = structuredOutput::stringConstants::userGroups.option;
  constexpr auto usersOpt = structuredOutput::stringConstants::users.option;
  constexpr auto groupsPerUserOpt = structuredOutput::stringConstants::groupsPerUser.option;
  const auto scriptPrintFilter = values.getOptional<std::string>("script-print");
  const auto preferredFormat = values.get<std::string>("format");

  structuredOutput::DisplayConfig config;
  config.flags = Flags::printHeaders * !scriptPrintFilter
               | Flags::printGroups * (!scriptPrintFilter || *scriptPrintFilter == userGroupsOpt)
  //groupsPerUser is a part of the users list. So when groupsPerUsers is requested, we must also print users
               | Flags::printUsers * (!scriptPrintFilter || *scriptPrintFilter == usersOpt || *scriptPrintFilter == groupsPerUserOpt)
               | Flags::printUserGroups * (!scriptPrintFilter || *scriptPrintFilter == groupsPerUserOpt);
  config.preferredFormat = (preferredFormat == "json") ? Format::json : Format::yaml;
  return config;
}

pep::AsaQuery CommandAsa::CommandAsaQuery::extractQuery(const pep::commandline::NamedValues& values) {
  return {
      .mAt = pep::Timestamp(values.get<int64_t>("at")),
      .mGroupFilter = values.get<std::string>("group"),
      .mUserFilter = values.get<std::string>("user"),
  };
}
