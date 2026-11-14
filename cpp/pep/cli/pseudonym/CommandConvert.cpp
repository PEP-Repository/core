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
#include <pep/ticketing/ColumnAccessModes.hpp>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/rx-observable.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pep::cli {

namespace {

namespace param {

constexpr auto from = "from";
constexpr auto to = "to";

} // namespace param

enum class IdType {
  PolymorphicPseudonym,
  LocalPseudonym,
  BriefLocalPseudonym,
};

/// The names by which a pseudonym type can be specified on the command line
struct IdTypeNames final {
  IdType type;
  std::string_view name; ///< Also used as key in structured output
  std::string_view abbreviation;
};

constexpr std::array<IdTypeNames, 3> idTypeNames{{
    {IdType::PolymorphicPseudonym, "polymorphic-pseudonym", "pp"},
    {IdType::LocalPseudonym, "local-pseudonym", "lp"},
    {IdType::BriefLocalPseudonym, "brief-local-pseudonym", "blp"},
}};

/// All names and abbreviations, sorted for the command line help
std::vector<std::string> AllIdTypeNames() {
  std::vector<std::string> result;
  for (const auto& names : idTypeNames) {
    result.emplace_back(names.name);
    result.emplace_back(names.abbreviation);
  }
  std::ranges::sort(result);
  return result;
}

IdType ParseIdTypeFromString(std::string str) {
  for (char& c: str) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
  const auto found = std::ranges::find_if(idTypeNames, [&str](const IdTypeNames& names) {
    return names.name == str || names.abbreviation == str;
  });
  if (found == idTypeNames.end()) { throw std::runtime_error{"Cannot parse IdType from " + str}; }
  return found->type;
}

std::string IdTypeToString(IdType type) {
  const auto found = std::ranges::find(idTypeNames, type, &IdTypeNames::type);
  if (found == idTypeNames.end()) { throw std::runtime_error{"Unsupported IdType"}; }
  return std::string(found->name);
}

struct Configuration final {
  std::string from;
  std::vector<IdType> to; ///< Without duplicates, in the order specified by the user
  structuredOutput::FormatConfig formatConfig;

  [[nodiscard]] bool includes(IdType type) const { return std::ranges::contains(to, type); }

  static Configuration From(const pep::commandline::NamedValues &values) {
    Configuration result{
      .from = values.get<std::string>(param::from),
      .to = {}, // Filled below
      .formatConfig = structuredOutput::FormatConfigFrom(values),
    };
    for (const auto& str : values.getOptionalMultiple<std::string>(param::to)) {
      if (const auto type = ParseIdTypeFromString(str); !result.includes(type)) {
        result.to.push_back(type);
      }
    }
    if (result.to.empty()) { // No --to: convert to all types
      std::ranges::transform(idTypeNames, std::back_inserter(result.to), &IdTypeNames::type);
    }
    return result;
  }
};

/// Prints the requested pseudonyms, in the order the user specified them
void ReportPseudonyms(const Configuration& config, const std::map<IdType, std::string>& pseudonyms) {
  auto result = nlohmann::ordered_json::object();
  for (auto type : config.to) {
    result[IdTypeToString(type)] = pseudonyms.at(type);
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
                .allow(AllIdTypeNames())) +
         structuredOutput::FormatParameter(structuredOutput::FormatFlags::Json | structuredOutput::FormatFlags::Yaml, "json");
}

int CommandPseudonym::CommandConvert::execute() {
  const auto config = Configuration::From(this->getParameterValues());

  return executeEventLoopFor([config](std::shared_ptr<pep::CoreClient> client) {
    return client->parsePPorIdentity(config.from).flat_map([client, config](PolymorphicPseudonym pp) {
      auto pseudonyms = std::make_shared<std::map<IdType, std::string>>();
      (*pseudonyms)[IdType::PolymorphicPseudonym] = pp.text();
      if (!config.includes(IdType::LocalPseudonym) && !config.includes(IdType::BriefLocalPseudonym)) {
        ReportPseudonyms(config, *pseudonyms);
        return rxcpp::observable<>::just(FakeVoid()).as_dynamic();
      }

      pep::RequestTicket2Opts tOpts;
      tOpts.referenceSubjects = {pp};
      tOpts.modes = ColumnAccessModes::Read;
      tOpts.includeAccessGroupPseudonyms = true;

      return client->requestTicket2(tOpts)
          .flat_map([client, config, pseudonyms](pep::IndexedTicket2 ticket) {
            const auto opened = ticket.openTicketWithoutCheckingSignature();
            const auto& subjects = opened->referenceSubjects;
            if (subjects.empty() || !subjects.front().accessGroup.has_value()) {
              throw std::runtime_error{"No localized pseudonym available for the supplied polymorphic pseudonym"};
            }

            const auto lp = client->decryptLocalPseudonym(*subjects.front().accessGroup);
            (*pseudonyms)[IdType::LocalPseudonym] = lp.text();
            if (!config.includes(IdType::BriefLocalPseudonym)) {
              ReportPseudonyms(config, *pseudonyms);
              return rxcpp::observable<>::just(FakeVoid()).as_dynamic();
            }

            return client->getGlobalConfiguration()
                .map([config, lp, pseudonyms](std::shared_ptr<GlobalConfiguration> gc) {
                  (*pseudonyms)[IdType::BriefLocalPseudonym] = gc->getUserPseudonymFormat().makeUserPseudonym(lp);
                  ReportPseudonyms(config, *pseudonyms);
                  return FakeVoid();
                }).as_dynamic();
          }).as_dynamic();
    });
  });
}

} // namespace pep::cli
