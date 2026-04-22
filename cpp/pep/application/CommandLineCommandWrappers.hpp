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
    : ChildCommandOf<TParent>(name, description, parent), mDeprecationMessage(std::move(deprecationMessage)) {}

  std::optional<std::string> getDeprecationWarning() const override { return mDeprecationMessage; }

private:
  std::string mDeprecationMessage;
};

template <typename TParent>
class AliasCommand : public ChildCommandOf<TParent> {
public:
  /// \param ancestor  The common ancestor command from which \p childPath is navigated.
  ///                  Typically \p parent, but may be any ancestor in the tree.
  /// \param childPath Subcommand tokens to navigate from \p ancestor to the target.
  ///                  May be empty if \p ancestor itself is the target.
  explicit AliasCommand(TParent& parent, const std::string& aliasName,
      Command& ancestor, CommandPath childPath,
      std::function<std::queue<std::string>(std::queue<std::string>)> transformer = nullptr,
      std::function<NamedValues(std::queue<std::string>)> valuesTransformer = nullptr)
    : ChildCommandOf<TParent>(aliasName, "Alias for: " + childPath.toString(), parent),
      mAncestor(ancestor),
      mChildPath(std::move(childPath)),
      mArgumentTransformer(std::move(transformer)),
      mValuesTransformer(std::move(valuesTransformer)) {}

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
    if (mValuesTransformer) {
      leafValues = mValuesTransformer(std::move(forwarded));
    }
    if (mArgumentTransformer) {
      forwarded = mArgumentTransformer(std::move(forwarded));
    }
    // Dispatch from the ancestor using its already-parsed values; forwarded args are lexed by the leaf.
    return mAncestor.dispatchTo(mChildPath, std::move(leafValues), std::move(forwarded));
  }

private:
  Command& mAncestor;
  CommandPath mChildPath;
  std::function<std::queue<std::string>(std::queue<std::string>)> mArgumentTransformer;
  std::function<NamedValues(std::queue<std::string>)> mValuesTransformer;
};

template <typename TParent>
class DeprecatedCommand : public AliasCommand<TParent> {
public:
  explicit DeprecatedCommand(TParent& parent, const std::string& aliasName,
      Command& ancestor, CommandPath childPath,
      const std::string& deprecationMessage,
      std::function<std::queue<std::string>(std::queue<std::string>)> transformer = nullptr,
      std::function<NamedValues(std::queue<std::string>)> valuesTransformer = nullptr)
    : AliasCommand<TParent>(parent, aliasName, ancestor, std::move(childPath), std::move(transformer), std::move(valuesTransformer)),
      mDeprecationMessage(deprecationMessage) {}

  std::optional<std::string> getDeprecationWarning() const override { return mDeprecationMessage; }

private:
  std::string mDeprecationMessage;
};

template <typename TParent>
class NoLongerSupportedCommand : public ChildCommandOf<TParent> {
public:
  explicit NoLongerSupportedCommand(TParent& parent, const std::string& name, const std::string& message)
    : ChildCommandOf<TParent>(name, "No longer supported. " + message, parent), mMessage(message) {}

  bool isUndocumented() const override { return true; }

  Parameters getSupportedParameters() const override {
    return ChildCommandOf<TParent>::getSupportedParameters()
      + Parameter("ignored", std::nullopt).value(Value<std::string>().positional().multiple());
  }

  int execute() override {
    std::cerr << "Error: The command '" << this->getName() << "' is no longer supported.";
    if (!mMessage.empty()) {
      std::cerr << ' ' << mMessage;
    }
    std::cerr << std::endl;
    return EXIT_FAILURE;
  }

private:
  std::string mMessage;
};

template <typename TParent>
std::shared_ptr<ChildCommandOf<TParent>> CreateAliasCommand(
    TParent& parent,
    const std::string& aliasName,
    Command& ancestor,
    CommandPath childPath,
    std::function<std::queue<std::string>(std::queue<std::string>)> transformer = nullptr,
    std::function<NamedValues(std::queue<std::string>)> valuesTransformer = nullptr) {
  return std::make_shared<AliasCommand<TParent>>(parent, aliasName, ancestor, std::move(childPath), std::move(transformer), std::move(valuesTransformer));
}

template <typename TParent>
std::shared_ptr<ChildCommandOf<TParent>> CreateDeprecatedCommand(
    TParent& parent,
    const std::string& aliasName,
    Command& ancestor,
    CommandPath childPath,
    const std::string& deprecationMessage,
    std::function<std::queue<std::string>(std::queue<std::string>)> transformer = nullptr,
    std::function<NamedValues(std::queue<std::string>)> valuesTransformer = nullptr) {
  return std::make_shared<DeprecatedCommand<TParent>>(parent, aliasName, ancestor, std::move(childPath), deprecationMessage, std::move(transformer), std::move(valuesTransformer));
}

template <typename TParent>
std::shared_ptr<ChildCommandOf<TParent>> CreateNoLongerSupportedCommand(
    TParent& parent,
    const std::string& name,
    const std::string& message) {
  return std::make_shared<NoLongerSupportedCommand<TParent>>(parent, name, message);
}

} // namespace pep::commandline