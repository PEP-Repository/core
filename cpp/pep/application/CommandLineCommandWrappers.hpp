#pragma once

#include <pep/application/CommandLineCommand.hpp>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <queue>
#include <string>

namespace pep::commandline {

/*!
 * \brief Base class for a deprecated command that still executes normally, printing a warning on invocation.
 * \details Derive from this instead of ChildCommandOf when the command is deprecated but still functional.
 * Override getSupportedParameters() and execute() (or createChildCommands()) as usual.
 * The deprecation warning is printed on actual invocation; it is suppressed when --help is requested.
 * \tparam TParent The parent command type. Must inherit from Command.
 */
template <typename TParent>
class DeprecatedChildCommandOf : public ChildCommandOf<TParent> {
protected:
  DeprecatedChildCommandOf(const std::string& name, const std::string& description, TParent& parent, std::string deprecationMessage)
    : ChildCommandOf<TParent>(name, description, parent), deprecationMessage_(std::move(deprecationMessage)) {}

  std::optional<std::string> getDeprecationWarning() const override { return deprecationMessage_; }

private:
  std::string deprecationMessage_;
};

template <typename TParent>
class AliasCommand : public ChildCommandOf<TParent> {
public:
  /// \param ancestor  The common ancestor command from which \p childPath is navigated.
  ///                  Typically \p parent, but may be any ancestor in the tree.
  /// \param childPath Subcommand tokens to navigate from \p ancestor to the target.
  ///                  May be empty if \p ancestor itself is the target.
  AliasCommand(TParent& parent, const std::string& aliasName,
    Command& ancestor, CommandPath childPath,
    std::function<NamedValues(std::queue<std::string>&)> transformer = nullptr)
  : ChildCommandOf<TParent>(aliasName, "Alias for: " + childPath.toString(), parent),
    ancestor_(ancestor),
    childPath_(std::move(childPath)),
    transformer_(std::move(transformer)) {}

  bool isForwardingCommand() const override { return true; }

  Parameters getSupportedParameters() const override {
    return ChildCommandOf<TParent>::getSupportedParameters()
      + Parameter("forwarded", std::nullopt).value(Value<std::string>().positional().multiple());
  }

  int execute() override {
    std::queue<std::string> forwarded;
    for (const auto& v : this->getParameterValues().template getOptionalMultiple<std::string>("forwarded")) {
      forwarded.push(v);
    }

    NamedValues leafValues;
    if (transformer_) {
      leafValues = transformer_(forwarded);
    }
    // Dispatch from the ancestor using its already-parsed values.
    // leafValues contains pre-built parameter values, forwarded contains remaining args to be lexed by the leaf.
    return ancestor_.dispatchTo(childPath_, std::move(leafValues), std::move(forwarded));
  }

private:
  Command& ancestor_;
  CommandPath childPath_;
  std::function<NamedValues(std::queue<std::string>&)> transformer_;
};

template <typename TParent>
class DeprecatedCommand : public AliasCommand<TParent> {
public:
  DeprecatedCommand(TParent& parent, const std::string& aliasName,
    Command& ancestor, CommandPath childPath,
    const std::string& deprecationMessage,
    std::function<NamedValues(std::queue<std::string>&)> transformer = nullptr)
  : AliasCommand<TParent>(parent, aliasName, ancestor, std::move(childPath), std::move(transformer)),
    deprecationMessage_(deprecationMessage) {}

  std::optional<std::string> getDeprecationWarning() const override { return deprecationMessage_; }

private:
  std::string deprecationMessage_;
};

template <typename TParent>
class NoLongerSupportedCommand : public ChildCommandOf<TParent> {
public:
  NoLongerSupportedCommand(TParent& parent, const std::string& name, const std::string& message)
  : ChildCommandOf<TParent>(name, "No longer supported. " + message, parent), message_(message) {}

  bool isUndocumented() const override { return true; }
  bool isNoLongerSupported() const override { return true; }

  Parameters getSupportedParameters() const override {
    return ChildCommandOf<TParent>::getSupportedParameters()
      + Parameter("ignored", std::nullopt).value(Value<std::string>().positional().multiple());
  }

  int execute() override {
    std::cerr << "Error: The command '" << this->getName() << "' is no longer supported.";
    if (!message_.empty()) {
      std::cerr << ' ' << message_;
    }
    std::cerr << std::endl;
    return EXIT_FAILURE;
  }

private:
  std::string message_;
};

template <typename TParent>
std::shared_ptr<ChildCommandOf<TParent>> CreateAliasCommand(
    TParent& parent,
    const std::string& aliasName,
    Command& ancestor,
    CommandPath childPath,
    std::function<NamedValues(std::queue<std::string>&)> transformer = nullptr) {
  return std::make_shared<AliasCommand<TParent>>(parent, aliasName, ancestor, std::move(childPath), std::move(transformer));
}

template <typename TParent>
std::shared_ptr<ChildCommandOf<TParent>> CreateDeprecatedCommand(
    TParent& parent,
    const std::string& aliasName,
    Command& ancestor,
    CommandPath childPath,
    const std::string& deprecationMessage,
    std::function<NamedValues(std::queue<std::string>&)> transformer = nullptr) {
  return std::make_shared<DeprecatedCommand<TParent>>(parent, aliasName, ancestor, std::move(childPath), deprecationMessage, std::move(transformer));
}

template <typename TParent>
std::shared_ptr<ChildCommandOf<TParent>> CreateNoLongerSupportedCommand(
    TParent& parent,
    const std::string& name,
    const std::string& message) {
  return std::make_shared<NoLongerSupportedCommand<TParent>>(parent, name, message);
}

} // namespace pep::commandline
