#include <array>
#include <cstddef>
#include <optional>
#include <pep/async/FakeVoid.hpp>
#include <pep/client/Client.hpp>
#include <pep/cli/pseudonym/CommandConvert.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
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

constexpr std::size_t StringsPerIdType = 3;
const std::array<std::string, 3 * StringsPerIdType> IdTypeStrings{
    "pp", "polymorphic", "polymorphic-pseudonym",
    "lp", "local",       "local-pseudonym",
    "up", "user",        "user-pseudonym"};

std::optional<IdType> TryParseIdTypeFromString(std::string str){
  for (char& c: str) { c = static_cast<char>(std::tolower(c)); }
  const auto found = std::ranges::find(IdTypeStrings, str);
  if (found == IdTypeStrings.end()) { return std::nullopt; }
  return static_cast<IdType>(static_cast<int>(distance(IdTypeStrings.begin(), found)));
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
                .allow(IdTypeStrings)
                .multiple()
                .required());
}

int CommandPseudonym::CommandConvert::execute() {
  using namespace rxcpp::operators;

  return executeEventLoopFor(
      [config = Configuration::From(this->getParameterValues())](std::shared_ptr<pep::Client> client) {
          throw std::runtime_error{"Not implemented yet"};

         return rxcpp::observable<>::just(pep::FakeVoid());
       });
}

} // namespace pep::cli
