#include <boost/algorithm/string/join.hpp>
#include <pep/cli/user/CommandUserQuery.hpp>
#include <pep/cli/structuredoutput/TreeFromUserQueryResponse.hpp>

#include <pep/core-client/CoreClient.hpp>
#include <pep/structuredoutput/Json.hpp>
#include <pep/structuredoutput/Yaml.hpp>
#include <pep/structuredoutput/Tree.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <rxcpp/operators/rx-map.hpp>

namespace {

using namespace std::chrono;
using namespace pep::cli;
namespace so = pep::structuredOutput;

}

pep::commandline::Parameters CommandUser::CommandUserQuery::getSupportedParameters() const {
  const auto userGroupsOpt = std::string{so::queryKeys::userGroups.simple};
  const auto usersOpt = std::string{so::queryKeys::users.simple};
  const auto groupsPerUserOpt = std::string{so::queryKeys::groupsPerUser.simple};

  return ChildCommandOf<CommandUser>::getSupportedParameters()
       + pep::commandline::Parameter("script-print", "Prints specified type of data without pretty printing")
             .value(pep::commandline::Value<std::string>().allow(std::vector<std::string>({"all-user-group", "all-user", "groups-per-user"})))
              .noLongerSupported("The --script-print option is no longer supported. Use --include and --format options instead.")
       + pep::commandline::Parameter("include", "Prints only specified type of data.")
             .value(pep::commandline::Value<std::string>().allow(std::vector<std::string>({userGroupsOpt,
                                                                                           usersOpt,
                                                                                           groupsPerUserOpt})).multiple())
       + pep::commandline::Parameter("format", "The format of the output.")
             .value(pep::commandline::Value<std::string>()
                        .allow(std::vector<std::string>({"yaml", "json", "json-compact"}))
                        .defaultsTo("yaml"))
       + pep::commandline::Parameter("at", "Query for this timestamp (milliseconds since 1970-01-01 00:00:00 in UTC), defaults to now if omitted")
             .value(pep::commandline::Value<milliseconds::rep>())
       + pep::commandline::Parameter("group", "Match user groups containing this text").value(pep::commandline::Value<std::string>())
       + pep::commandline::Parameter("user", "Match user identifiers containing this text").value(pep::commandline::Value<std::string>());
}

int CommandUser::CommandUserQuery::execute() {
  return this->executeEventLoopFor([values = this->getParameterValues()](std::shared_ptr<pep::CoreClient> client) {
    return client->getAccessManagerProxy()->userQuery(extractQuery(values)).map([displayConfig = extractConfig(values)](pep::UserQueryResponse res) {
      auto tree = so::TreeFrom(res, displayConfig);
      
      if (displayConfig.format() == so::Format::Json) {
        so::json::append(std::cout, tree, std::get<so::JsonConfig>(displayConfig.formatConfig)) << std::endl;
      } else {
        so::yaml::append(std::cout, tree, std::get<so::YamlConfig>(displayConfig.formatConfig)) << std::endl;
      }

      // Warn for users without displayId
      auto usersWithoutDisplayId = res.mUsers | std::ranges::views::filter([](QRUser user){ return !user.mDisplayId; });
      for (auto& user : usersWithoutDisplayId) {
        auto uids = std::move(user.mOtherUids);
        if (user.mPrimaryId) {
          uids.push_back(*user.mPrimaryId);
        }
        LOG(LOG_TAG, warning) << "No display-id for user with identifiers: " << boost::algorithm::join(uids, ", ");
      }
      return pep::FakeVoid();
    });
  });
}

so::QueryDisplayConfig<so::UserQueryFlags> CommandUser::CommandUserQuery::extractConfig(const pep::commandline::NamedValues& values) {
  using FormatConfig = decltype(so::QueryDisplayConfig<so::UserQueryFlags>::formatConfig);
  using Flags = so::UserQueryFlags;

  const auto isIncluded = [includedTypes = values.getOptionalMultiple<std::string>("include")](const auto key) {
    return includedTypes.empty() || std::ranges::find(includedTypes, key.simple) != includedTypes.end();
  };
  const auto format = values.get<std::string>("format");

  return {
    .flags =
        FlagsIf(Flags::PrintUserGroups, isIncluded(so::queryKeys::userGroups)) |
        FlagsIf(Flags::PrintUsers, isIncluded(so::queryKeys::users)) |
        FlagsIf(Flags::PrintUserGroupsForUsers, isIncluded(so::queryKeys::groupsPerUser)),
    .useDescriptiveKeys = (format == "yaml"),
    .formatConfig = [&format] () -> FormatConfig {
        if (format == "json") {
          return so::JsonConfig{};
        } else if (format == "json-compact") {
          return so::JsonConfig{.wsformat = so::WhitespaceFormat::Compact};
        } else {
          return so::YamlConfig{.includeArraySizeComments = true, .includeEmptyArrayComments = true};
        }
    }(),
  };
}

pep::UserQuery CommandUser::CommandUserQuery::extractQuery(const pep::commandline::NamedValues& values) {
  return {
      .mAt = GetOptionalValue(values.getOptional<milliseconds::rep>("at"), [](milliseconds::rep ms) {
        return Timestamp(milliseconds{ms});
      }),
      .mGroupFilter = values.getOptional<std::string>("group").value_or(""),
      .mUserFilter = values.getOptional<std::string>("user").value_or(""),
  };
}
