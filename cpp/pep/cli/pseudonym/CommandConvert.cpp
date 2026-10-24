#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <pep/async/FakeVoid.hpp>
#include <pep/cli/pseudonym/CommandConvert.hpp>
#include <pep/cli/structuredoutput/FormatParameter.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <ranges>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/rx-observable.hpp>
#include <stdexcept>
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
  IdType to;
  std::optional<structuredOutput::FormatConfig> formatConfig; ///< Not set: print just the pseudonym

  static Configuration From(const pep::commandline::NamedValues &values) {
    return {
      .from = values.get<std::string>(param::from),
      .to = ParseIdTypeFromString(values.get<std::string>(param::to)),
      .formatConfig = values.has(structuredOutput::formatParameterName)
          ? std::make_optional(structuredOutput::FormatConfigFrom(values))
          : std::nullopt,
    };
  }
};

void ReportPseudonym(const Configuration& config, const std::string& pseudonym) {
  if (config.formatConfig.has_value()) {
    structuredOutput::WriteStructured(std::cout, structuredOutput::Tree::FromJson({{IdTypeToString(config.to), pseudonym}}), *config.formatConfig);
  }
  else {
    std::cout << pseudonym << std::endl;
  }
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
         Parameter(param::to, "Target pseudonym type")
             .value(Value<std::string>()
                .positional()
                .allow(AllIdTypeNames())
                .required()) +
         structuredOutput::FormatParameter(structuredOutput::FormatFlags::Json | structuredOutput::FormatFlags::Yaml, std::nullopt);
}

int CommandPseudonym::CommandConvert::execute() {
  const auto config = Configuration::From(this->getParameterValues());

  return executeEventLoopFor([config](std::shared_ptr<pep::CoreClient> client) {
    return client->parsePPorIdentity(config.from).flat_map([client, config](PolymorphicPseudonym pp) {
      if (config.to == IdType::PolymorphicPseudonym) {
        ReportPseudonym(config, pp.text());
        return rxcpp::observable<>::just(FakeVoid()).as_dynamic();
      }

      pep::RequestTicket2Opts tOpts;
      tOpts.referenceSubjects = {pp};
      tOpts.modes = {"read"};
      tOpts.includeAccessGroupPseudonyms = true;

      return client->requestTicket2(tOpts)
          .flat_map([client, config](pep::IndexedTicket2 ticket) {
            const auto opened = ticket.openTicketWithoutCheckingSignature();
            const auto& subjects = opened->referenceSubjects;
            if (subjects.empty() || !subjects.front().accessGroup.has_value()) {
              throw std::runtime_error{"No localized pseudonym available for the supplied polymorphic pseudonym"};
            }

            const auto lp = client->decryptLocalPseudonym(*subjects.front().accessGroup);
            if (config.to == IdType::LocalPseudonym) {
              ReportPseudonym(config, lp.text());
              return rxcpp::observable<>::just(FakeVoid()).as_dynamic();
            }

            return client->getGlobalConfiguration()
                .map([config, lp](std::shared_ptr<GlobalConfiguration> gc) {
                  ReportPseudonym(config, gc->getUserPseudonymFormat().makeUserPseudonym(lp));
                  return FakeVoid();
                }).as_dynamic();
          }).as_dynamic();
    });
  });
}

} // namespace pep::cli
