#include <pep/cli/pseudonym/CommandConvert.hpp>

#include <pep/accessmanager/AccessManagerMessages.hpp> // for IndexedTicket2
#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/application/CommandLineValue.hpp> // for commandline::NamedValues
#include <pep/async/FakeVoid.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/Pseudonym.hpp>
#include <pep/cli/structuredoutput/FormatParameter.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/structuredoutput/FormatFlags.hpp>
#include <pep/structuredoutput/Tree.hpp>
#include <pep/ticketing/ColumnAccessModes.hpp>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/rx-observable.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pep::cli {

namespace {

namespace param {

constexpr auto from = "from";
constexpr auto to = "to";

} // namespace param

enum class PseudonymType {
  PolymorphicPseudonym,
  LocalPseudonym,
  BriefLocalPseudonym,
};

/// The names by which a pseudonym type can be specified on the command line
struct PseudonymTypeNames final {
  PseudonymType type;
  std::string_view name; ///< Also used as key in structured output
  std::string_view abbreviation;
};

constexpr auto pseudonymTypeNames = std::array<PseudonymTypeNames, 3>{{
    {PseudonymType::PolymorphicPseudonym, "polymorphic-pseudonym", "pp"},
    {PseudonymType::LocalPseudonym, "local-pseudonym", "lp"},
    {PseudonymType::BriefLocalPseudonym, "brief-local-pseudonym", "blp"},
}};

/// All names and abbreviations, sorted for the command line help
std::vector<std::string> AllPseudonymTypeNames() {
  std::vector<std::string> result;
  result.reserve(pseudonymTypeNames.size() * 2);
  for (const auto& names : pseudonymTypeNames) {
    result.emplace_back(names.name);
    result.emplace_back(names.abbreviation);
  }
  std::ranges::sort(result);
  return result;
}

/// \param str A name or abbreviation: the command line parser only accepts values from AllPseudonymTypeNames
PseudonymType ParsePseudonymType(std::string_view str) {
  const auto found = std::ranges::find_if(pseudonymTypeNames, [str](const PseudonymTypeNames& names) {
    return names.name == str || names.abbreviation == str;
  });
  assert(found != pseudonymTypeNames.end());
  return found->type;
}

std::string_view NameOf(PseudonymType type) {
  const auto found = std::ranges::find(pseudonymTypeNames, type, &PseudonymTypeNames::type);
  assert(found != pseudonymTypeNames.end());
  return found->name;
}

/// The types specified with --to, without duplicates and in the specified order, or all types if --to was omitted
std::vector<PseudonymType> ParseTargetTypes(const commandline::NamedValues& values) {
  std::vector<PseudonymType> result;
  for (const auto& str : values.getOptionalMultiple<std::string>(param::to)) {
    if (const auto type = ParsePseudonymType(str); !std::ranges::contains(result, type)) {
      result.push_back(type);
    }
  }
  if (result.empty()) {
    std::ranges::transform(pseudonymTypeNames, std::back_inserter(result), &PseudonymTypeNames::type);
  }
  return result;
}

struct Configuration final {
  std::string from;
  std::vector<PseudonymType> to;
  structuredOutput::FormatConfig formatConfig;

  [[nodiscard]] bool includes(PseudonymType type) const { return std::ranges::contains(to, type); }

  static Configuration From(const commandline::NamedValues& values) {
    return {
      .from = values.get<std::string>(param::from),
      .to = ParseTargetTypes(values),
      .formatConfig = structuredOutput::FormatConfigFrom(values),
    };
  }
};

/// Retrieves the local pseudonym if a requested type needs it, or std::nullopt otherwise
rxcpp::observable<std::optional<LocalPseudonym>> RetrieveLocalPseudonymIfNeeded(
    std::shared_ptr<CoreClient> client, const Configuration& config, const PolymorphicPseudonym& pp) {
  if (!config.includes(PseudonymType::LocalPseudonym) && !config.includes(PseudonymType::BriefLocalPseudonym)) {
    return rxcpp::observable<>::just(std::optional<LocalPseudonym>()).as_dynamic();
  }

  RequestTicket2Opts tOpts;
  tOpts.referenceSubjects = {pp};
  tOpts.modes = ColumnAccessModes::Read;
  tOpts.includeAccessGroupPseudonyms = true;

  return client->requestTicket2(tOpts)
      .map([client](IndexedTicket2 ticket) -> std::optional<LocalPseudonym> {
        const auto opened = ticket.openTicketWithoutCheckingSignature();
        const auto& subjects = opened->referenceSubjects;
        if (subjects.empty() || !subjects.front().accessGroup.has_value()) {
          throw std::runtime_error{"No localized pseudonym available for the supplied polymorphic pseudonym"};
        }
        return client->decryptLocalPseudonym(*subjects.front().accessGroup);
      })
      .as_dynamic();
}

/// Retrieves the global configuration if a requested type needs it, or nullptr otherwise
rxcpp::observable<std::shared_ptr<GlobalConfiguration>> RetrieveGlobalConfigurationIfNeeded(
    std::shared_ptr<CoreClient> client, const Configuration& config) {
  if (!config.includes(PseudonymType::BriefLocalPseudonym)) {
    return rxcpp::observable<>::just(std::shared_ptr<GlobalConfiguration>()).as_dynamic();
  }
  return client->getGlobalConfiguration().as_dynamic();
}

/// Converts \p pp to \p type, using the local pseudonym and global configuration retrieved for the requested types
std::string Convert(PseudonymType type, const PolymorphicPseudonym& pp, const std::optional<LocalPseudonym>& lp,
                    const std::shared_ptr<GlobalConfiguration>& gc) {
  switch (type) {
  case PseudonymType::PolymorphicPseudonym:
    return pp.text();
  case PseudonymType::LocalPseudonym:
    return lp.value().text();
  case PseudonymType::BriefLocalPseudonym:
    return gc->getUserPseudonymFormat().makeUserPseudonym(lp.value());
  }
  throw std::logic_error{"Unsupported pseudonym type"};
}

/// Prints \p pp converted to the requested types, in the order the user specified them
void ReportPseudonyms(const Configuration& config, const PolymorphicPseudonym& pp,
                      const std::optional<LocalPseudonym>& lp, const std::shared_ptr<GlobalConfiguration>& gc) {
  auto result = nlohmann::ordered_json::object();
  for (auto type : config.to) {
    result[std::string(NameOf(type))] = Convert(type, pp, lp, gc);
  }
  structuredOutput::WriteStructured(std::cout, structuredOutput::Tree::FromJson(std::move(result)), config.formatConfig);
}

} // namespace

commandline::Parameters
CommandPseudonym::CommandConvert::getSupportedParameters() const {
  using namespace commandline;
  return ChildCommandOf<CommandPseudonym>::getSupportedParameters() +
         Parameter(param::from, "Pseudonym value to convert")
             .value(Value<std::string>()
                .positional()
                .required()) +
         Parameter(param::to, "Target pseudonym type (can be repeated). Omit to convert to all types.")
             .value(Value<std::string>()
                .multiple()
                .allow(AllPseudonymTypeNames())) +
         structuredOutput::FormatParameter(structuredOutput::FormatFlags::Json | structuredOutput::FormatFlags::Yaml, "json");
}

int CommandPseudonym::CommandConvert::execute() {
  const auto config = Configuration::From(this->getParameterValues());

  return executeEventLoopFor([config](std::shared_ptr<CoreClient> client) {
    return client->parsePPorIdentity(config.from).flat_map([client, config](PolymorphicPseudonym pp) {
      return RetrieveLocalPseudonymIfNeeded(client, config, pp)
          .flat_map([client, config, pp](std::optional<LocalPseudonym> lp) {
            return RetrieveGlobalConfigurationIfNeeded(client, config)
                .map([config, pp, lp](std::shared_ptr<GlobalConfiguration> gc) {
                  ReportPseudonyms(config, pp, lp, gc);
                  return FakeVoid();
                });
          });
    });
  });
}

} // namespace pep::cli
