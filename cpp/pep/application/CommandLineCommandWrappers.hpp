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
  /// \param childPath Space-separated subcommand tokens to navigate from \p ancestor to the target.
  ///                  May be empty if \p ancestor itself is the target.
  explicit AliasCommand(TParent& parent, const std::string& aliasName,
      Command& ancestor, const std::string& childPath,
      std::function<std::queue<std::string>(std::queue<std::string>)> transformer = nullptr)
    : ChildCommandOf<TParent>(aliasName, "Alias for: " + childPath, parent),
      mAncestor(ancestor),
      mChildPath(childPath),
      mArgumentTransformer(std::move(transformer)) {}

  bool isUndocumented() const override { return true; }

  Parameters getSupportedParameters() const override {
    return ChildCommandOf<TParent>::getSupportedParameters()
      + Parameter("forwarded", "Forwards all arguments to the target command.").value(Value<std::string>().positional().multiple());
  }

  int execute() override {
    std::queue<std::string> forwarded;
    for (const auto& v : this->getParameterValues().template getOptionalMultiple<std::string>("forwarded")) {
      forwarded.push(v);
    }
    if (mArgumentTransformer) {
      forwarded = mArgumentTransformer(std::move(forwarded));
    }
    // Dispatch from the ancestor using its already-parsed values; forwarded args are lexed by the leaf.
    return mAncestor.dispatchTo(mChildPath, {}, std::move(forwarded));
  }

private:
  Command& mAncestor;
  std::string mChildPath;
  std::function<std::queue<std::string>(std::queue<std::string>)> mArgumentTransformer;
};

template <typename TParent>
class DeprecatedCommand : public AliasCommand<TParent> {
public:
  explicit DeprecatedCommand(TParent& parent, const std::string& aliasName,
      Command& ancestor, const std::string& childPath,
      const std::string& deprecationMessage,
      std::function<std::queue<std::string>(std::queue<std::string>)> transformer = nullptr)
    : AliasCommand<TParent>(parent, aliasName, ancestor, childPath, std::move(transformer)),
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
    const std::string& childPath,
    std::function<std::queue<std::string>(std::queue<std::string>)> transformer = nullptr) {
  return std::make_shared<AliasCommand<TParent>>(parent, aliasName, ancestor, childPath, std::move(transformer));
}

template <typename TParent>
std::shared_ptr<ChildCommandOf<TParent>> CreateDeprecatedCommand(
    TParent& parent,
    const std::string& aliasName,
    Command& ancestor,
    const std::string& childPath,
    const std::string& deprecationMessage,
    std::function<std::queue<std::string>(std::queue<std::string>)> transformer = nullptr) {
  return std::make_shared<DeprecatedCommand<TParent>>(parent, aliasName, ancestor, childPath, deprecationMessage, std::move(transformer));
}

template <typename TParent>
std::shared_ptr<ChildCommandOf<TParent>> CreateNoLongerSupportedCommand(
    TParent& parent,
    const std::string& name,
    const std::string& message) {
  return std::make_shared<NoLongerSupportedCommand<TParent>>(parent, name, message);
}

} // namespace pep::commandline