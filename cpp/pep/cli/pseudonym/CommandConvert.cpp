#include <boost/container/flat_map.hpp>
#include <optional>
#include <pep/async/FakeVoid.hpp>
#include <pep/cli/pseudonym/CommandConvert.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <ranges>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/rx-observable.hpp>
#include <stdexcept>

namespace pep::cli {

namespace {

namespace cliParameterNames {

constexpr auto from = "from";
constexpr auto to = "to";

} // namespace cliParameterNames

enum class IdType {
  PolymorphicPseudonym,
  LocalPseudonym,
  UserPseudonym,
};

const auto stringToIdType = boost::container::flat_map<std::string, IdType> {
    {"pp", IdType::PolymorphicPseudonym},
    {"polymorphic", IdType::PolymorphicPseudonym},
    {"polymorphic-pseudonym", IdType::PolymorphicPseudonym},
    {"lp", IdType::LocalPseudonym},
    {"local", IdType::LocalPseudonym},
    {"local-pseudonym", IdType::LocalPseudonym},
    {"up", IdType::UserPseudonym},
    {"user", IdType::UserPseudonym},
    {"user-pseudonym", IdType::UserPseudonym}
};

std::optional<IdType> TryParseIdTypeFromString(std::string str){
  for (char& c: str) { c = static_cast<char>(std::tolower(c)); }
  const auto found = stringToIdType.find(str);
  if (found == stringToIdType.end()) { return std::nullopt; }
  return found->second;
}

IdType ParseIdTypeFromString(const std::string& str){
  const auto parsed = TryParseIdTypeFromString(str);
  if (!parsed.has_value()) throw std::runtime_error{"Cannot parse IdType from " + str};
  return *parsed;
}

struct Configuration final {
  std::string from;
  IdType to;

  static Configuration From(const pep::commandline::NamedValues &values) {
    namespace param = cliParameterNames;
    return {
      .from = values.get<std::string>(param::from),
      .to = ParseIdTypeFromString(values.get<std::string>(param::to)),
    };
  }
};

} // namespace

commandline::Parameters
CommandPseudonym::CommandConvert::getSupportedParameters() const {
  using namespace commandline;
  namespace param = cliParameterNames;
  return ChildCommandOf<CommandPseudonym>::getSupportedParameters() +
         Parameter(param::from, "Pseudonym value to convert")
             .value(Value<std::string>()
                .positional()
                .required()) +
         Parameter(param::to, "Target pseudonym type")
             .value(Value<std::string>()
                .positional()
                .allow(stringToIdType | std::views::keys)
                .required());
}

int CommandPseudonym::CommandConvert::execute() {
  const auto config = Configuration::From(this->getParameterValues());

  return executeEventLoopFor([config](std::shared_ptr<pep::CoreClient> client) {
    return client->parsePPorIdentity(config.from).flat_map([client, config](PolymorphicPseudonym pp) {
      if (config.to == IdType::PolymorphicPseudonym) {
        std::cout << pp.text() << std::endl;
        return rxcpp::observable<>::just(FakeVoid()).as_dynamic();
      }

      pep::RequestTicket2Opts tOpts;
      tOpts.pps = {pp};
      tOpts.modes = {"read"};
      tOpts.includeAccessGroupPseudonyms = true;

      return client->requestTicket2(tOpts)
          .flat_map([client, config](pep::IndexedTicket2 ticket) {
            const auto opened = ticket.openTicketWithoutCheckingSignature();
            const auto& subjects = opened->accessSubjects_;
            if (subjects.empty() || !subjects.front().accessGroup_.has_value()) {
              throw std::runtime_error{"No localized pseudonym available for the supplied polymorphic pseudonym"};
            }

            const auto lp = client->decryptLocalPseudonym(*subjects.front().accessGroup_);
            if (config.to == IdType::LocalPseudonym) {
              std::cout << lp.text() << std::endl;
              return rxcpp::observable<>::just(FakeVoid()).as_dynamic();
            }

            return client->getGlobalConfiguration()
                .map([config, lp](std::shared_ptr<GlobalConfiguration> gc) {
                  std::cout << gc->getUserPseudonymFormat().makeUserPseudonym(lp) << std::endl;
                  return FakeVoid();
                }).as_dynamic();
          }).as_dynamic();
    });
  });
}

} // namespace pep::cli
