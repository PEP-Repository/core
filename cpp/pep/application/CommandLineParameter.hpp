#pragma once

#include <pep/application/CommandLineSwitchAnnouncement.hpp>
#include <pep/application/CommandLineValue.hpp>
#include <pep/application/CommandLineValueSpecification.hpp>
#include <pep/utils/Shared.hpp>
#include <functional>
#include <initializer_list>
#include <queue>
#include <set>
#include <optional>
#include <string>
#include <vector>

namespace pep {
namespace commandline {

class Parameters;
class Command;

struct CommandPath {
  std::vector<std::string> segments;

  CommandPath() = default;
  CommandPath(std::initializer_list<std::string> parts) : segments(parts) {}
  explicit CommandPath(std::vector<std::string> parts) : segments(std::move(parts)) {}

  bool empty() const noexcept { return segments.empty(); }
  std::string toString() const;

  bool operator==(const CommandPath&) const = default;
};

/*!
 * \brief Result of a parameter transformation/forwarding operation.
 * \details Describes what to add at the target and where to dispatch to.
 * `toAdd` contains values to inject at the target (the original param itself is erased by the framework).
 * `ancestor` is the command from which `childPath` is navigated; nullptr means dispatch from self.
 * `childPath` is a typed sequence of subcommand names to navigate from `ancestor` to the target.
 * An empty `childPath` means the ancestor itself is the target.
 */
struct ParameterTransformationResult {
  NamedValues toAdd;
  Command* ancestor = nullptr; // nullptr = dispatch from self
  CommandPath childPath;       // empty = target is ancestor/self

  explicit ParameterTransformationResult(NamedValues toAdd, Command* ancestor = nullptr, CommandPath childPath = {})
    : toAdd(std::move(toAdd)), ancestor(ancestor), childPath(std::move(childPath)) {}
};

/*!
 * \brief Definition of a formal parameter belonging to a command.
 * \details Parameters may accept a value (via `.value(...)`) or just be present as a switch or not.
 * Parameters accepting a value may be positional (have no associated switch).
 * Besides its canonical name, a non-positional parameter may have aliases.
 */
class Parameter {
  friend class Parameters;

private:
  std::string mName;
  std::optional<std::string> mDescription;
  std::set<SwitchAnnouncement> mAliases;
  std::shared_ptr<ValueSpecificationBase> mValueSpecification;

  // Deprecation state: at most one of these is set
  std::optional<std::string> mDeprecationMessage;
  std::optional<std::string> mNoLongerSupportedMessage;
  std::function<ParameterTransformationResult(Command&, const NamedValues&)> mTransformer;

  Parameter alias(const SwitchAnnouncement& alias) const;
  std::optional<std::string> getInvocationSummary(const std::string& prefix, const std::string& identifier, bool indicateOptionality) const;

  void lex(ProvidedValues& destination, std::queue<std::string>& source) const;
  void finalize(Values& parsed) const;

  bool isLackingValue(const ProvidedValues& lexed) const noexcept;

  std::optional<std::string> getInvocationSummary(bool indicateOptionality) const;
  std::unordered_map<SwitchAnnouncement, std::string> getAliasInvocationSummaries() const;
  void writeHelpText(std::ostream& destination) const;

public:
  Parameter(const std::string& name, const std::optional<std::string>& description); // Omit the description to create an undocumented switch

  inline Parameter alias(const std::string& name) const { return this->alias(SwitchAnnouncement(name)); }
  inline Parameter shorthand(char shorthand) const { return this->alias(SwitchAnnouncement(shorthand)); }

  // Forwarding aliases: transform/redirect parameters
  // Custom transformer receives fully-parsed+finalized values, may redirect to a different command
  Parameter forwardingAlias(std::function<ParameterTransformationResult(Command&, const NamedValues&)> transformer) const;
  // Rename: automatically moves the value to newParamName and shows deprecation warning
  Parameter rename(const std::string& newParamName) const;
  // Only show deprecation warning, combine with forwardingAlias() to also transform/redirect parameters
  Parameter deprecated(const std::string& message) const;
  // Marks a parameter as completely removed, print error and exit
  Parameter noLongerSupported(const std::string& message) const;

  template <typename T>
  Parameter value(Value<T> value) const;

  const std::string& getName() const noexcept { return mName; }
  const std::optional<std::string>& getDescription() const noexcept { return mDescription; }

  SwitchAnnouncement getCanonicalAnnouncement() const;
  std::set<SwitchAnnouncement> getAnnouncements() const;
  std::shared_ptr<const ValueSpecificationBase> getValueSpecification() const noexcept { return mValueSpecification; }
  [[nodiscard]] bool hasTransformer() const noexcept { return mTransformer != nullptr; }
  [[nodiscard]] bool isNoLongerSupported() const noexcept { return mNoLongerSupportedMessage.has_value(); }
  [[nodiscard]] bool isDeprecated() const noexcept { return mDeprecationMessage.has_value(); }
  [[nodiscard]] const std::optional<std::string>& getNoLongerSupportedMessage() const noexcept { return mNoLongerSupportedMessage; }
  [[nodiscard]] const std::optional<std::string>& getDeprecationMessage() const noexcept { return mDeprecationMessage; }
  ParameterTransformationResult transform(Command& self, const NamedValues& values) const;

  bool isRequired() const noexcept;
  bool isPositional() const noexcept;
  bool allowsMultiple() const noexcept;
  bool isDocumented() const noexcept { return mDescription.has_value() && !this->isDeprecated() && !this->isNoLongerSupported(); }

  Values parse(const ProvidedValues& lexed) const;
};

/*!
 * \brief Parameters belonging to one (sub)command.
 * These stop at the first unrecognized argument or "--".
 */
class Parameters {
  friend class Command;

private:
  std::vector<Parameter> mEntries;
  using Index = typename std::vector<Parameter>::size_type;
  /// Non-positional parameters
  std::vector<Index> mNamed;
  /// Positional parameters
  std::vector<Index> mPositional;
  /// All parameters, including positional
  std::unordered_map<std::string, Index> mByAnnouncement;

  void add(const Parameter& parameter);
  void writeHelpText(std::ostream& destination, const std::string& header, std::vector<Index> indices) const;

  /*!
   * \param terminated If non-null, will be set to true if lexing stopped after a `SwitchAnnouncement::STOP_PROCESSING` token
   */
  LexedValues lex(std::queue<std::string>& arguments, bool* terminated = nullptr) const;
  NamedValues parse(const LexedValues& lexed) const;
  void finalize(NamedValues& parsed) const;

  /*!
   * \brief Get switch requiring a value at this position (`--switch <here>`), if any.
   */
  const Parameter* currentSwitchRequiringValue(const LexedValues& lexed) const noexcept;

  /*!
   * \brief Get the first positional parameter from this position accepting a value.
   */
  const Parameter* firstPositional(const LexedValues& lexed) const noexcept;

  /*!
   * \brief Get parameters for which switches should be completed at this position.
   */
  std::vector<const Parameter*> getSwitchesToAutocomplete(const LexedValues& lexed) const noexcept;

  inline bool empty() const { return mEntries.empty(); }
  bool hasRequired() const;
  /*!
   * \brief Is there a positional parameter accepting multiple arguments?
   */
  bool hasInfinitePositional() const noexcept;
  std::vector<std::string> getInvocationSummary() const;
  void writeHelpText(std::ostream& destination) const;

  inline auto begin() const { return mEntries.cbegin(); }
  inline auto end() const { return mEntries.cend(); }

public:
  Parameters operator +(const Parameter& parameter) const;
  Parameters operator +(const std::vector<Parameter>& parameters) const;
  inline Parameters operator +(const Parameters& parameters) const { return *this + parameters.mEntries; }

  const Parameter* find(const std::string& name) const;
};

template <typename T>
Parameter Parameter::value(Value<T> value) const {
  if (mValueSpecification != nullptr) {
    throw std::runtime_error("A value has already been specified for command line switch " + mName);
  }
  try {
    value.validate();
  }
  catch (const std::exception& error) {
    throw std::runtime_error("Parameter '" + mName + "': " + error.what());
  }

  auto result = *this;
  result.mValueSpecification = MakeSharedCopy(value);
  return result;
}

}
}
