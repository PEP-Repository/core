#pragma once

#include <pep/application/CommandLineSwitchAnnouncement.hpp>
#include <pep/application/CommandLineValueSpecification.hpp>
#include <pep/utils/Shared.hpp>
#include <queue>
#include <set>

namespace pep {
namespace commandline {

class Parameters;
class Command;

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

  template <typename T>
  Parameter value(Value<T> value) const;

  const std::string& getName() const noexcept { return mName; }
  const std::optional<std::string>& getDescription() const noexcept { return mDescription; } 

  std::optional<SwitchAnnouncement> getCanonicalAnnouncement() const;
  std::set<SwitchAnnouncement> getAnnouncements() const;
  std::shared_ptr<const ValueSpecificationBase> getValueSpecification() const noexcept { return mValueSpecification; }

  bool isRequired() const noexcept;
  bool isPositional() const noexcept;
  bool allowsMultiple() const noexcept;
  bool isDocumented() const noexcept { return mDescription.has_value(); }

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
  std::vector<Index> mNamed;
  std::unordered_map<SwitchAnnouncement, Index> mByAnnouncement;
  std::vector<Index> mPositional;

  void add(const Parameter& parameter);
  void writeHelpText(std::ostream& destination, const std::string& header, std::vector<Index> indices) const;

  /*!
   * \param terminated If non-null, will be set to true if lexing stopped after a `SwitchAnnouncement::STOP_PROCESSING` token
   */
  LexedValues lex(std::queue<std::string>& arguments, bool* terminated = nullptr) const;
  NamedValues parse(const LexedValues& lexed) const;
  void finalize(NamedValues& parsed) const;

  /*!
   * \brief Get the current parameter accepting a value
   */
  const Parameter* firstAcceptingValue(const LexedValues& lexed) const noexcept;
  /*!
   * \brief Get parameters for which switches, or values for positional parameters, should be completed here
   */
  std::vector<const Parameter*> getParametersToAutocomplete(const LexedValues& lexed) const noexcept;

  inline bool empty() const { return mEntries.empty(); }
  bool hasRequired() const;
  /*!
   * \brief Is there a positional parameter accepting multiple arguments?
   */
  bool hasInfinitePositional() const noexcept;
  std::vector<std::string> getInvocationSummary() const;
  void writeHelpText(std::ostream& destination) const;

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
    if (value.isPositional() && !mAliases.empty()) {
      throw std::runtime_error("Aliased parameter cannot be made positional");
    }
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
