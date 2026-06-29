#include <pep/application/CommandLineParameter.hpp>

#include <numeric>
#include <utility>
#include <iostream>

#include <boost/algorithm/string/join.hpp>

namespace pep {
namespace commandline {

std::string CommandPath::toString() const {
  return boost::algorithm::join(segments, " ");
}

Parameter::Parameter(const std::string& name, const std::optional<std::string>& description)
  : name_(name), description_(description) {
  assert(description == std::nullopt || !description->empty());
}

Parameter Parameter::alias(const SwitchAnnouncement& alias) const {
  auto announcements = this->getAnnouncements();
  if (announcements.find(alias) != announcements.cend()) {
    throw std::runtime_error("Switch " + name_ + " already has announcement " + alias.string());
  }

  Parameter result = *this;
  [[maybe_unused]] auto emplaced = result.aliases_.emplace(alias).second;
  assert(emplaced);
  return result;
}

Parameter Parameter::forwardingAlias(std::function<ParameterTransformationResult(Command&, const NamedValues&)> transformer) const {
  assert(transformer && "Forwarding transformer for parameter must be provided");
  assert(!noLongerSupportedMessage_.has_value() && "Cannot combine forwardingAlias() and noLongerSupported() on the same parameter");
  assert(!transformer_ && "Cannot apply multiple transformers to the same parameter");

  auto result = *this;
  result.transformer_ = std::move(transformer);
  return result;
}

Parameter Parameter::rename(const std::string& newParamName) const {
  assert(!noLongerSupportedMessage_.has_value() && "Cannot combine rename() and noLongerSupported() on the same parameter");
  assert(!transformer_ && "Cannot apply multiple transformers to the same parameter");
  assert(!deprecationMessage_.has_value() && "Cannot apply deprecated() before rename() on the same parameter");

  const std::string oldName = name_;
  return this->forwardingAlias([oldName, newParamName](Command&, const NamedValues& values) {
    NamedValues toAdd;
    toAdd.set(newParamName, values.at(oldName));
    return ParameterTransformationResult{std::move(toAdd)};
  }).deprecated("Use --" + newParamName + " instead.");
}

Parameter Parameter::deprecated(const std::string& message) const {
  assert(!noLongerSupportedMessage_.has_value() && "Cannot combine deprecated() and noLongerSupported() on the same parameter");
  assert(!deprecationMessage_.has_value() && "Cannot apply deprecated() multiple times to the same parameter");

  auto result = *this;
  result.deprecationMessage_ = message;
  return result;
}

Parameter Parameter::noLongerSupported(const std::string& message) const {
  assert(!transformer_ && "Cannot combine forwardingAlias() and noLongerSupported() on the same parameter");

  auto result = *this;
  result.noLongerSupportedMessage_ = message;
  return result;
}

SwitchAnnouncement Parameter::getCanonicalAnnouncement() const {
  return SwitchAnnouncement(name_);
}

std::set<SwitchAnnouncement> Parameter::getAnnouncements() const {
  auto result = aliases_;
  [[maybe_unused]] auto emplaced = result.emplace(this->getCanonicalAnnouncement()).second;
  assert(emplaced);
  return result;
}

ParameterTransformationResult Parameter::transform(Command& self, const NamedValues& values) const {
  assert(transformer_ && "No transformer configured for parameter");
  return transformer_(self, values);
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
      throw std::runtime_error("Parameter '" + name_ + "' provided multiple times");
    }
    for (const auto& unparsed : lexed) {
      if (!unparsed.has_value()) {
        throw std::runtime_error("Parameter '" + name_ + "' requires a value but none was provided");
      }
      try {
        result.add(v->parse(*unparsed));
      }
      catch (const std::exception& error) {
        throw std::runtime_error("Parameter '" + name_ + "': " + error.what());
      }
    }
  }
  else {
    if (lexed.size() > 1U) {
      throw std::runtime_error("Parameter '" + name_ + "' provided multiple times");
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
      throw std::runtime_error("Parameter '" + name_ + "': " + error.what());
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
  if (this->isPositional()) {
    return this->getInvocationSummary(std::string(), name_, indicateOptionality);
  }
  auto announcement = this->getCanonicalAnnouncement();
  return this->getInvocationSummary(announcement.getPrefix(), announcement.getText(), indicateOptionality);
}

std::unordered_map<SwitchAnnouncement, std::string> Parameter::getAliasInvocationSummaries() const {
  std::unordered_map<SwitchAnnouncement, std::string> result;
  if (!this->isDocumented()) {
    return result;
  }

  for (const auto& alias : aliases_) {
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
    return optionalityLeft + name_ + further + optionalityRight; // [name] or [name [...]] or <name> or <name [...]>
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

    assert(description_.has_value());
    assert(summary.has_value());
    WriteHelpItem(destination, *summary, *description_);

    auto v = this->getValueSpecification();
    if (v != nullptr) {
      v->writeHelpText(destination);
    }
  }
}

void Parameters::writeHelpText(std::ostream& destination) const {
  this->writeHelpText(destination, "Switches", named_);
  this->writeHelpText(destination, "Parameters", positional_);

  using Entry = std::tuple<SwitchAnnouncement, std::string, std::string>;
  std::vector<Entry> entries;
  for (const auto& parameter : entries_) {
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
  auto pos = std::find_if(entries_.cbegin(), entries_.cend(), [&name](const Parameter& candidate) {return candidate.getName() == name; });
  if (pos == entries_.cend()) {
    return nullptr;
  }
  return &*pos;
}

void Parameters::writeHelpText(std::ostream& destination, const std::string& header, std::vector<Index> indices) const {
  struct CompareParameterNamesByIndex {
    const std::vector<Parameter>& parameters;
    bool operator()(Index lhs, Index rhs) const { return std::less<std::string>()(parameters[lhs].getName(), parameters[rhs].getName()); }
  };
  std::sort(indices.begin(), indices.end(), CompareParameterNamesByIndex{ entries_ });

  auto announce = true;
  for (auto index : indices) {
    const auto& parameter = entries_[index];
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
  auto index = entries_.size();

  // Exception safe update: create a copy of our announcements to work with
  auto byAnnouncement = byAnnouncement_;
  for (const auto& announcement : parameter.getAnnouncements()) {
    auto emplaced = byAnnouncement.emplace(announcement.string(), index);
    if (!emplaced.second) {
      auto existing = entries_[emplaced.first->second];
      throw std::runtime_error("Announcement " + announcement.string() + " is claimed by multiple switches: " + parameter.getName() + " and " + existing.getName());
    }
  }
  // No conflicting announcements: update our state
  (parameter.isPositional() ? positional_ : named_).push_back(index);
  std::swap(byAnnouncement_, byAnnouncement);

  entries_.push_back(parameter);
}

LexedValues Parameters::lex(std::queue<std::string>& arguments, bool* const terminated) const {
  if (terminated) *terminated = false;
  LexedValues result;

  auto positionalIt = positional_.cbegin();
  enum class PositionalType { None, Named, Unnamed } positionalSeen = PositionalType::None;

  while (!arguments.empty()) {
    const auto& token = arguments.front();

    if (token == SwitchAnnouncement::STOP_PROCESSING) {
      arguments.pop(); // discard the STOP_PROCESSING token from remaining arguments
      if (terminated) *terminated = true;
      break;
    }

    const Parameter* s{};
    if (auto named = byAnnouncement_.find(token); named != byAnnouncement_.cend()) { // The current token is a "--name" or "-shorthand" announcement
      arguments.pop(); // Discard the announcement from remaining arguments
      s = &entries_[named->second];
      if (s->isPositional()) {
        if (positionalSeen == PositionalType::Unnamed) {
          throw std::runtime_error("Cannot mix named and unnamed positional parameters");
        }
        positionalSeen = PositionalType::Named;
      }
    }
    else { // Not an announcement: process as an unnamed positional parameter
      if (positionalIt == positional_.cend()) {
        // We don't support any further positionals, so the token is not for us.
        break;
      }

      if (positionalSeen == PositionalType::Named) {
        if (!this->firstPositional(result)) {
          // All positionals were already specified using names. Assume the next token is not for us.
          break;
        }
        throw std::runtime_error("Cannot mix named and unnamed positional parameters");
      }
      positionalSeen = PositionalType::Unnamed;

      s = &entries_[*positionalIt];
      assert(s->isPositional());

      auto valueSpec = s->getValueSpecification();
      assert(valueSpec && "Positional without value specification?");
      if (!valueSpec->allowsMultiple()) {
        ++positionalIt;
      }
    }

    s->lex(result[s->getName()], arguments);
  }

  return result;
}

NamedValues Parameters::parse(const LexedValues& lexed) const {
  NamedValues result;

  for (const auto& s : entries_) {
    const auto& name = s.getName();
    auto position = lexed.find(name);
    if (position != lexed.cend()) {
      result.set(name, s.parse(position->second));
    }
  }

  return result;
}

void Parameters::finalize(NamedValues& parsed) const {
  for (const auto& s : entries_) {
    const auto& name = s.getName();
    if (!parsed.has(name)) {
      Values tmp;
      s.finalize(tmp);
      if (!tmp.empty()) {
        parsed.set(name, tmp);
      }
    }
  }
}

const Parameter* Parameters::currentSwitchRequiringValue(const LexedValues& lexed) const noexcept {
  for (const Parameter& s : entries_) {
    // If specified without value
    if (auto position = lexed.find(s.getName()); position != lexed.cend()) {
      if (s.isLackingValue(position->second)) {
        return &s;
      }
    }
  }
  return {};
}

const Parameter* Parameters::firstPositional(const LexedValues& lexed) const noexcept {
  for (const Index i : positional_) {
    const Parameter& s = entries_[i];
    if (!lexed.contains(s.getName()) || s.allowsMultiple()) {
      return &s;
    }
  }
  return {};
}

std::vector<const Parameter*> Parameters::getSwitchesToAutocomplete(const LexedValues& lexed) const noexcept {
  std::vector<const Parameter*> params;
  for (const auto& param : entries_) {
    if (!param.isDocumented()) {
      continue;
    }
    const auto valueSpec = param.getValueSpecification();
    if (!lexed.contains(param.getName()) || (valueSpec && valueSpec->allowsMultiple())) {
      params.push_back(&param);
    }
  }
  return params;
}

bool Parameters::hasRequired() const {
  return std::any_of(entries_.cbegin(), entries_.cend(), [](const Parameter& s) {return s.isRequired(); });
}

bool Parameters::hasInfinitePositional() const noexcept {
  return std::any_of(entries_.cbegin(), entries_.cend(), [](const Parameter& s) {return s.allowsMultiple(); });
}

std::vector<std::string> Parameters::getInvocationSummary() const {
  std::vector<std::optional<std::string>> optionals;
  optionals.reserve(entries_.size());

  auto position = std::back_inserter(optionals);
  position = std::transform(named_.cbegin(), named_.cend(), position, [this](Index index) {return entries_[index].getInvocationSummary(true); });
  std::transform(positional_.cbegin(), positional_.cend(), position, [this](Index index) {return entries_[index].getInvocationSummary(true); });

  optionals.erase(std::remove(optionals.begin(), optionals.end(), std::nullopt), optionals.end());

  std::vector<std::string> result;
  result.reserve(optionals.size());
  std::transform(optionals.cbegin(), optionals.cend(), std::back_inserter(result), [](const std::optional<std::string> entry) {return *entry; });
  return result;
}

}
}
