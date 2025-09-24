#pragma once

#include <pep/application/CommandLineParameter.hpp>
#include <functional>

namespace pep {
namespace commandline {

/*!
 * \brief One (sub)command, with parameters directly following it.
 * Contains formal definition and possibly the concrete values.
 */
class Command {
private:
  std::optional<NamedValues> mParameterValues;
  bool mParametersLexed = false;
  bool mParametersFinalized = false;

  int issueCommandLineHelp(const std::optional<std::string>& error);
  int printAutocompleteInfo(std::queue<std::string>& arguments);
  int autocompleteChildCommand(std::queue<std::string>& arguments);
  bool hasRequiredArgument();

protected:
  virtual std::optional<int> processLexedParameters(const LexedValues& lexed); // Overrides must call base implementation
  virtual void finalizeParameters(); // Overrides must call base implementation, which applies defaults

  const NamedValues& getParameterValues() const; // Invokable (during or) after invocation of this->finalizeParameters

  // TODO: don't require override-of-one-of-two-methods
  virtual std::vector<std::shared_ptr<Command>> createChildCommands() { return {}; } // Derived classes should override either "createChildCommands" or "execute" but not both
  virtual int execute(); // Derived classes should override either "createChildCommands" or "execute" but not both

public:
  virtual ~Command() noexcept = default;
  int process(std::queue<std::string>& arguments);

  virtual std::string getName() const = 0;
  virtual std::string getDescription() const = 0;
  virtual bool isUndocumented() const { return false; }
  virtual std::optional<std::string> getAdditionalDescription() const { return std::nullopt; }
  virtual std::optional<std::string> getRelativeDocumentationUrl() const { return std::nullopt; }
  virtual const Command* getParentCommand() const noexcept { return nullptr; }
  virtual Parameters getSupportedParameters() const; // Derived classes should add to this set
};

/*!
 * \brief Utility base for child commands.
 * \tparam TParent The parent command type. Must inherit from Command.
 */
template <typename TParent>
class ChildCommandOf : public Command {
private:
  std::string mName;
  std::string mDescription;
  TParent& mParent;

protected:
  ChildCommandOf(const std::string& name, const std::string& description, TParent& parent);

  inline std::string getName() const override { return mName; }
  inline std::string getDescription() const override { return mDescription; }
  inline const Command* getParentCommand() const noexcept override { return &mParent; }

  inline TParent& getParent() noexcept {
    assert(&mParent == this->getParentCommand());
    return mParent;
  }

  inline const TParent& getParent() const noexcept {
    assert(&mParent == this->getParentCommand());
    return mParent;
  }
};

template <typename TParent>
ChildCommandOf<TParent>::ChildCommandOf(const std::string& name, const std::string& description, TParent& parent)
  : mName(name), mDescription(description), mParent(parent) {
  static_assert(std::is_base_of<Command, TParent>::value);
  assert(!mName.empty());
  assert(!mDescription.empty());
}

}
}
