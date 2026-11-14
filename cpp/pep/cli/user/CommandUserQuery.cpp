#include <pep/cli/user/CommandUserQuery.hpp>

#include <pep/cli/structuredoutput/FormatParameter.hpp>
#include <pep/cli/structuredoutput/TreeFromUserQueryResponse.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/accessmanager/UserMessages.hpp> // for UserQueryResponse, QRUser
#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/cli/CliApplication.hpp> // for cli::LogTag
#include <pep/cli/Command.hpp>
#include <pep/cli/User.hpp>
#include <pep/structuredoutput/Common.hpp> // for structuredOutput::queryKeys::userGroups, structuredOutput::JsonConfig, structuredOutput::YamlConfig, ...
#include <pep/utils/EnumUtils.hpp> // for FlagsIf
#include <pep/utils/Log.hpp> // for Severity
#include <pep/utils/Timestamp.hpp>

#include <rxcpp/operators/rx-map.hpp>

#include <boost/algorithm/string/join.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <ranges>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace std::chrono;
using namespace pep::cli;
namespace so = pep::structuredOutput;

}

using namespace std::ranges;

pep::commandline::Parameters CommandUser::CommandUserQuery::getSupportedParameters() const {
  const auto userGroupsOpt = std::string{so::queryKeys::userGroups.simple};
  const auto usersOpt = std::string{so::queryKeys::users.simple};
  const auto groupsPerUserOpt = std::string{so::queryKeys::groupsPerUser.simple};

  return ChildCommandOf<CommandUser>::getSupportedParameters()
       + pep::commandline::Parameter("script-print", "Prints specified type of data without pretty printing")
             .value(pep::commandline::Value<std::string>().allow(std::vector<std::string>({"all-user-group", "all-user", "groups-per-user"})))
              .noLongerSupported("Use --include and --format options instead.")
       // NOTE: the "user group query" alias forwards into this command by injecting "include"
       + pep::commandline::Parameter("include", "Prints only specified type of data.")
             .value(pep::commandline::Value<std::string>().allow(std::vector<std::string>({userGroupsOpt,
                                                                                           usersOpt,
                                                                                           groupsPerUserOpt})).multiple())
       + so::FormatParameter()
       + pep::commandline::Parameter("point-in-time", "Query for the situation at the specified point-in-time. Defaults to now if omitted")
             .value(pep::commandline::Value<Timestamp>())
       + pep::commandline::Parameter("group", "Match user groups containing this text").value(pep::commandline::Value<std::string>())
       + pep::commandline::Parameter("user", "Match user identifiers containing this text").value(pep::commandline::Value<std::string>());
}

int CommandUser::CommandUserQuery::execute() {
  return this->executeEventLoopFor([values = this->getParameterValues()](std::shared_ptr<pep::CoreClient> client) {
    return client->getAccessManagerProxy()->userQuery(extractQuery(values)).map([displayConfig = extractConfig(values)](pep::UserQueryResponse res) {
      so::WriteStructured(std::cout, so::TreeFrom(res, displayConfig), displayConfig.formatConfig);

      // Warn for users without displayId
      auto usersWithoutDisplayId = res.users | views::filter([](QRUser user){ return !user.displayId; });
      for (auto& user : usersWithoutDisplayId) {
        auto uids = std::move(user.otherUids);
        if (user.primaryId) {
          uids.push_back(*user.primaryId);
        }
        PEP_LOG(LogTag, Severity::Warning) << "No display-id for user with identifiers: " << boost::algorithm::join(uids, ", ");
      }
      return pep::FakeVoid();
    });
  });
}

so::QueryDisplayConfig<so::UserQueryFlags> CommandUser::CommandUserQuery::extractConfig(const pep::commandline::NamedValues& values) {
  using Flags = so::UserQueryFlags;

  const auto isIncluded = [includedTypes = values.getOptionalMultiple<std::string>("include")](const auto key) {
    return includedTypes.empty() || contains(includedTypes, key.simple);
  };
  const auto formatConfig = so::FormatConfigFrom(values, so::YamlConfig{.includeArraySizeComments = true, .includeEmptyArrayComments = true});

  return {
    .flags =
        FlagsIf(Flags::PrintUserGroups, isIncluded(so::queryKeys::userGroups)) |
        FlagsIf(Flags::PrintUsers, isIncluded(so::queryKeys::users)) |
        FlagsIf(Flags::PrintUserGroupsForUsers, isIncluded(so::queryKeys::groupsPerUser)),
    .useDescriptiveKeys = std::holds_alternative<so::YamlConfig>(formatConfig),
    .formatConfig = formatConfig,
  };
}

pep::UserQuery CommandUser::CommandUserQuery::extractQuery(const pep::commandline::NamedValues& values) {
  return {
      .at = values.getOptional<Timestamp>("point-in-time"),
      .groupFilter = values.getOptional<std::string>("group").value_or(""),
      .userFilter = values.getOptional<std::string>("user").value_or(""),
  };
}
