#include <pep/application/CommandLineParameter.hpp>

#include <numeric>
#include <utility>


#include <boost/algorithm/string/join.hpp>

namespace pep {
namespace commandline {

Parameter::Parameter(const std::string& name, const std::optional<std::string>& description)
  : mName(name), mDescription(description) {
  assert(description == std::nullopt || !description->empty());
}

Parameter Parameter::alias(const SwitchAnnouncement& alias) const {
  if (this->isPositional()) {
    throw std::runtime_error("Cannot add alias to parameter " + mName + " because it's positional");
  }
  auto announcements = this->getAnnouncements();
  if (announcements.find(alias) != announcements.cend()) {
    throw std::runtime_error("Switch " + mName + " already has announcement " + alias.string());
  }

  Parameter result = *this;
  [[maybe_unused]] auto emplaced = result.mAliases.emplace(alias).second;
  assert(emplaced);
  return result;
}

std::optional<SwitchAnnouncement> Parameter::getCanonicalAnnouncement() const {
  if (this->isPositional()) {
    return std::nullopt;
  }
  return SwitchAnnouncement(mName);
}

std::set<SwitchAnnouncement> Parameter::getAnnouncements() const {
  auto result = mAliases;
  auto canonical = this->getCanonicalAnnouncement();
  if (canonical.has_value()) {
    [[maybe_unused]] auto emplaced = result.emplace(*canonical).second;
    assert(emplaced);
  }
  return result;
}

void Parameter::lex(ProvidedValues& destination, std::queue<std::string>& source) const {
  auto v = this->getValueSpecification();
  if (v == nullptr || source.empty()) {
    destination.push_back(std::nullopt);
  }
  else if(v->eatsAll()) {
    source = std::queue<std::string>();
  }
  else {
    auto value = source.front();
    source.pop();
    destination.push_back(value);
  }
}

Values Parameter::parse(const ProvidedValues& lexed) const {
  Values result;

  auto v = this->getValueSpecification();
  if (v != nullptr) {
    if (lexed.size() > 1U && !v->allowsMultiple()) {
      throw std::runtime_error("Parameter '" + mName + "' provided multiple times");
    }
    for (const auto& unparsed : lexed) {
      if (!unparsed.has_value()) {
        throw std::runtime_error("Parameter '" + mName + "' requires a value but none was provided");
      }
      try {
        result.add(v->parse(*unparsed));
      }
      catch (const std::exception& error) {
        throw std::runtime_error("Parameter '" + mName + "': " + error.what());
      }
    }
  }
  else {
    if (lexed.size() > 1U) {
      throw std::runtime_error("Parameter '" + mName + "' provided multiple times");
    }
    result.add(std::nullopt);
  }

  return result;
}

bool Parameter::isLackingValue(const ProvidedValues& lexed) const noexcept {
  if (this->getValueSpecification()) {
    return std::any_of(lexed.cbegin(), lexed.cend(), [](const ProvidedValue &val) {
      return !val.has_value();
    });
  }
  return false;
}

void Parameter::finalize(Values& destination) const {
  auto v = this->getValueSpecification();
  if (v != nullptr) {
    try {
      v->finalize(destination);
    }
    catch (const std::runtime_error& error) {
      throw std::runtime_error("Parameter '" + mName + "': " + error.what());
    }
  }
}

bool Parameter::isRequired() const noexcept {
  auto value = this->getValueSpecification();
  if (value == nullptr) {
    return false;
  }
  return value->isRequired();
}

bool Parameter::isPositional() const noexcept {
  auto value = this->getValueSpecification();
  if (value == nullptr) {
    return false;
  }
  return value->isPositional();
}

bool Parameter::allowsMultiple() const noexcept {
  auto value = this->getValueSpecification();
  if (value == nullptr) {
    return false;
  }
  return value->allowsMultiple();
}

std::optional<std::string> Parameter::getInvocationSummary(bool indicateOptionality) const {
  auto announcement = this->getCanonicalAnnouncement();
  if (announcement.has_value()) {
    return this->getInvocationSummary(announcement->getPrefix(), announcement->getText(), indicateOptionality);
  }
  return this->getInvocationSummary(std::string(), mName, indicateOptionality);
}

std::unordered_map<SwitchAnnouncement, std::string> Parameter::getAliasInvocationSummaries() const {
  std::unordered_map<SwitchAnnouncement, std::string> result;
  if (!this->isDocumented()) {
    return result;
  }

  for (const auto& alias : mAliases) {
    auto entry = this->getInvocationSummary(alias.getPrefix(), alias.getText(), false);
    assert(entry.has_value());
    result.emplace(alias, *entry);
  }
  return result;
}

std::optional<std::string> Parameter::getInvocationSummary(const std::string& prefix, const std::string& identifier, bool indicateOptionality) const {
  if (!this->isDocumented()) {
    return std::nullopt;
  }

  std::string optionalityLeft, optionalityRight;
  if (indicateOptionality) {
    optionalityLeft = '[';
    optionalityRight = ']';
  }
  std::string announcement = prefix + identifier;

  auto v = this->getValueSpecification();
  if (v == nullptr) {
    return optionalityLeft + announcement + optionalityRight;
  }

  if (indicateOptionality && v->isRequired()) {
    optionalityLeft = '<';
    optionalityRight = '>';
  }
  std::string further;
  if (v->allowsMultiple()) {
    further = " [...]";
  }

  if (v->isPositional()) {
    return optionalityLeft + mName + further + optionalityRight; // [name] or [name [...]] or <name> or <name [...]>
  }

  auto result = announcement + " <value>";
  if (v->allowsMultiple()) {
    result += " [" + result + " ...]";
  }
  if (indicateOptionality && !v->isRequired()) {
    result = optionalityLeft + result + optionalityRight;
  }

  return result;
}

void Parameter::writeHelpText(std::ostream& destination) const {
  if (this->isDocumented()) {
    auto summary = this->getInvocationSummary(false);

    assert(mDescription.has_value());
    assert(summary.has_value());
    WriteHelpItem(destination, *summary, *mDescription);

    auto v = this->getValueSpecification();
    if (v != nullptr) {
      v->writeHelpText(destination);
    }
  }
}

void Parameters::writeHelpText(std::ostream& destination) const {
  this->writeHelpText(destination, "Switches", mNamed);
  this->writeHelpText(destination, "Parameters", mPositional);

  using Entry = std::tuple<SwitchAnnouncement, std::string, std::string>;
  std::vector<Entry> entries;
  for (const auto& parameter : mEntries) {
    auto canonical = parameter.getInvocationSummary(false);
    for (const auto& alias : parameter.getAliasInvocationSummaries()) {
      assert(canonical.has_value());
      entries.push_back(std::make_tuple(alias.first, alias.second, *canonical));
    }
  }
  struct CompareAliasText {
    inline bool operator()(const Entry& lhs, const Entry& rhs) const {
      const auto& left = std::get<0>(lhs).getText();
      const auto& right = std::get<0>(rhs).getText();
      return std::less<std::string>()(left, right);
    }
  };
  std::sort(entries.begin(), entries.end(), CompareAliasText());

  bool announce = true;
  for (const auto& entry : entries) {
    if (announce) {
      destination << "\nSwitch aliases: \n";
      announce = false;
    }
    WriteHelpItem(destination, std::get<1>(entry), "Alias for " + std::get<2>(entry)); // TODO: indent so that text for --proper-aliases and -shorthands is aligned
  }
}

Parameters Parameters::operator +(const Parameter& parameter) const {
  auto result = *this;
  result.add(parameter);
  return result;
}

Parameters Parameters::operator +(const std::vector<Parameter>& parameters) const {
  auto result = *this;
  for (const auto& parameter : parameters) {
    result.add(parameter);
  }
  return result;
}

const Parameter* Parameters::find(const std::string& name) const {
  auto pos = std::find_if(mEntries.cbegin(), mEntries.cend(), [&name](const Parameter& candidate) {return candidate.getName() == name; });
  if (pos == mEntries.cend()) {
    return nullptr;
  }
  return &*pos;
}

void Parameters::writeHelpText(std::ostream& destination, const std::string& header, std::vector<Index> indices) const {
  struct CompareParameterNamesByIndex {
    const std::vector<Parameter>& parameters;
    bool operator()(Index lhs, Index rhs) const { return std::less<std::string>()(parameters[lhs].getName(), parameters[rhs].getName()); }
  };
  std::sort(indices.begin(), indices.end(), CompareParameterNamesByIndex{ mEntries });

  auto announce = true;
  for (auto index : indices) {
    const auto& parameter = mEntries[index];
    if (parameter.isDocumented()) {
      if (announce) {
        destination << '\n' << header << ":\n";
        announce = false;
      }
      parameter.writeHelpText(destination);
    }
  }
}

void Parameters::add(const Parameter& parameter) {
  auto index = mEntries.size();

  if (parameter.isPositional()) {
    mPositional.push_back(index);
  }
  else {
    // Exception safe update: create a copy of our announcements to work with
    auto byAnnouncement = mByAnnouncement;
    for (const auto& announcement : parameter.getAnnouncements()) {
      auto emplaced = byAnnouncement.emplace(announcement, index);
      if (!emplaced.second) {
        auto existing = mEntries[emplaced.first->second];
        throw std::runtime_error("Announcement " + announcement.string() + " is claimed by multiple switches: " + parameter.getName() + " and " + existing.getName());
      }
    }
    // No conflicting announcements: update our state
    mNamed.push_back(index);
    std::swap(mByAnnouncement, byAnnouncement);
  }

  mEntries.push_back(parameter);
}

LexedValues Parameters::lex(std::queue<std::string>& arguments, bool* const terminated) const {
  if (terminated) *terminated = false;
  LexedValues result;

  auto positional = mPositional.cbegin(), positionals_end = mPositional.cend();

  while (!arguments.empty()) {
    const auto& token = arguments.front();

    if (token == SwitchAnnouncement::STOP_PROCESSING) {
      arguments.pop(); // discard the STOP_PROCESSING token from remaining arguments
      if (terminated) *terminated = true;
      break;
    }

    const Parameter* s{};
    auto named = std::find_if(mByAnnouncement.cbegin(), mByAnnouncement.cend(), [&token](const auto& pair) {return pair.first.string() == token; });
    if (named != mByAnnouncement.cend()) { // The current token is a "--name" or "-shorthand" announcement
      arguments.pop(); // Discard the announcement from remaining arguments
      auto index = named->second;
      s = &mEntries[index];
    }
    else { // Not an announcement: process as a positional parameter
      if (positional == positionals_end) { // We don't support any further positionals, so the token is not for us
        break;
      }
      auto index = *positional;
      s = &mEntries[index];
      // If the current positional can be specified only once, advance to the next positional
      assert(s->getValueSpecification() != nullptr);
      if (!s->getValueSpecification()->allowsMultiple()) {
        ++positional;
      }
    }
    s->lex(result[s->getName()], arguments);
  }

  return result;
}

NamedValues Parameters::parse(const LexedValues& lexed) const {
  NamedValues result;

  for (const auto& s : mEntries) {
    const auto& name = s.getName();
    auto position = lexed.find(name);
    if (position != lexed.cend()) {
      result[name] = s.parse(position->second);
    }
  }

  return result;
}

void Parameters::finalize(NamedValues& parsed) const {
  for (const auto& s : mEntries) {
    const auto& name = s.getName();
    if (!parsed.has(name)) {
      Values tmp;
      s.finalize(tmp);
      if (!tmp.empty()) {
        parsed[name] = tmp;
      }
    }
  }
}

const Parameter* Parameters::firstAcceptingValue(const LexedValues& lexed) const noexcept {
  // First check switches, then positional,
  //  as a specified switch with missing value must first get a value
  //  (note that there can only be one such switch)
  // (Positional parameters are already sorted in mEntries)
  for (bool positional = false; ; positional = true) {
    for (const auto& s : mEntries) {
      if (s.isPositional() != positional) {
        continue;
      }
      const auto& name = s.getName();
      auto position = lexed.find(name);
      if (position != lexed.cend()) {
        if (s.isLackingValue(position->second)) {
          return &s;
        }
      }
    }
    if (positional) {
      return {};
    }
  }
}

std::vector<const Parameter*> Parameters::getParametersToAutocomplete(const LexedValues& lexed) const noexcept {
  std::vector<const Parameter*> params;
  // Only return the first positional accepting values
  bool positionalAutocompleteSeen = false;
  for (const auto& param : mEntries) {
    if (!param.isDocumented()) {
      continue;
    }
    const auto& name = param.getName();
    auto position = lexed.find(name);
    const auto valueSpec = param.getValueSpecification();
    if (position == lexed.end() || (valueSpec && valueSpec->allowsMultiple())) {
      if (!param.isPositional() || !std::exchange(positionalAutocompleteSeen, true)) {
        params.push_back(&param);
      }
    }
  }
  return params;
}

bool Parameters::hasRequired() const {
  return std::any_of(mEntries.cbegin(), mEntries.cend(), [](const Parameter& s) {return s.isRequired(); });
}

bool Parameters::hasInfinitePositional() const noexcept {
  return std::any_of(mEntries.cbegin(), mEntries.cend(), [](const Parameter& s) {return s.allowsMultiple(); });
}

std::vector<std::string> Parameters::getInvocationSummary() const {
  std::vector<std::optional<std::string>> optionals;
  optionals.reserve(mEntries.size());

  auto position = std::back_inserter(optionals);
  position = std::transform(mNamed.cbegin(), mNamed.cend(), position, [this](Index index) {return mEntries[index].getInvocationSummary(true); });
  std::transform(mPositional.cbegin(), mPositional.cend(), position, [this](Index index) {return mEntries[index].getInvocationSummary(true); });

  optionals.erase(std::remove(optionals.begin(), optionals.end(), std::nullopt), optionals.end());

  std::vector<std::string> result;
  result.reserve(optionals.size());
  std::transform(optionals.cbegin(), optionals.cend(), std::back_inserter(result), [](const std::optional<std::string> entry) {return *entry; });
  return result;
}

}
}
