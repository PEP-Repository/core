#pragma once

#include <pep/application/CommandLineParameter.hpp>
#include <functional>
#include <string>
#include <vector>

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

  std::optional<int> applyParameterTransformations(const Parameters& parameters, std::queue<std::string>& remainingArgs, bool isLeafDispatch = false);

  int routeToDescendant(CommandPath childPath, NamedValues leafValues, std::queue<std::string> leafArgs);

protected:
  virtual std::optional<int> processLexedParameters(const LexedValues& lexed); // Overrides must call base implementation
  virtual void finalizeParameters(); // Overrides must call base implementation, which applies defaults

  // TODO: don't require override-of-one-of-two-methods
  virtual std::vector<std::shared_ptr<Command>> createChildCommands() { return {}; } // Derived classes should override either "createChildCommands" or "execute" but not both
  virtual int execute(); // Derived classes should override either "createChildCommands" or "execute" but not both

public:
  virtual ~Command() noexcept = default;
  int process(std::queue<std::string>& arguments, bool isLeafDispatch = false, std::optional<NamedValues> preMergedValues = std::nullopt);

  virtual std::string getName() const = 0;
  virtual std::string getDescription() const = 0;
  virtual bool isUndocumented() const { return false; }
  virtual std::optional<std::string> getAdditionalDescription() const { return std::nullopt; }
  virtual std::optional<std::string> getRelativeDocumentationUrl() const { return std::nullopt; }
  /// Override to emit a deprecation warning when the command is invoked
  virtual std::optional<std::string> getDeprecationWarning() const { return std::nullopt; }
  /// Returns true if this command forwards to another command (alias/forwarding)
  virtual bool isForwardingCommand() const { return false; }
  /// Returns true if this command is no longer supported
  virtual bool isNoLongerSupported() const { return false; }
  virtual const Command* getParentCommand() const noexcept { return nullptr; }
  virtual Parameters getSupportedParameters() const; // Derived classes should add to this set
  const NamedValues& getParameterValues() const; // Available after finalizeParameters() has been called

  /*!
   * \brief Dispatch to a (possibly nested) descendant using pre-built values, without re-lexing ancestor args.
   * \details Routing steps finalize this level and navigate into the named child. At the leaf,
   * `leafValues` are merged in, `leafArgs` are lexed, then the command is finalized and executed.
   * Use this for alias forwarding and parameter deprecation.
   * \param childPath Subcommand names to navigate to the target. Empty means this command is the target.
   * \param leafValues Values to merge into the target (leaf) command before finalizing.
   * \param leafArgs   Raw arguments to lex into the target command before finalizing.
   */
  int dispatchTo(CommandPath childPath, NamedValues leafValues, std::queue<std::string> leafArgs = {});
};

/*!
 * \brief Utility base for child commands.
 * \tparam TParent The parent command type. Must inherit from Command.
 */
template <typename TParent>
class ChildCommandOf : public Command {
private:
  std::string name_;
  std::string description_;
  TParent& parent_;

protected:
  ChildCommandOf(const std::string& name, const std::string& description, TParent& parent);

  inline std::string getName() const override { return name_; }
  inline std::string getDescription() const override { return description_; }
  inline const Command* getParentCommand() const noexcept override { return &parent_; }

  inline TParent& getParent() noexcept {
    assert(&parent_ == this->getParentCommand());
    return parent_;
  }

  inline const TParent& getParent() const noexcept {
    assert(&parent_ == this->getParentCommand());
    return parent_;
  }
};

template <typename TParent>
ChildCommandOf<TParent>::ChildCommandOf(const std::string& name, const std::string& description, TParent& parent)
  : name_(name), description_(description), parent_(parent) {
  static_assert(std::is_base_of<Command, TParent>::value);
  assert(!name_.empty());
  assert(!description_.empty());
}

}
}
