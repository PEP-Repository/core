#pragma once

#include <pep/application/CommandLineCommand.hpp>
#include <pep/application/CommandLineCommandWrappers.hpp>
#include <pep/application/CommandLineValue.hpp>
#include <pep/application/CommandLineParameter.hpp>

#include <functional>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <optional>

namespace pep::application::test {

class AppCmd;
class ServerCmd;

std::queue<std::string> ToQueue(std::initializer_list<std::string> values);
std::pair<int, std::string> ProcessWithCapturedStderr(AppCmd& cmd, std::initializer_list<std::string> args);

// Mixin for automatic parameter recording in test commands
// This is needed because child commands go out of scope after process() returns.
// The mixin captures their parameter values during execute() and stores them in the root AppCmd.
class RecordingCommandMixin {
protected:
  void captureParameters();
  ~RecordingCommandMixin() = default;

protected:
  virtual pep::commandline::Command* getCommand() = 0;
  virtual AppCmd& getStorage() = 0;
  virtual RecordingCommandMixin* getParentMixin() { return nullptr; }
};

class UserCmd : public pep::commandline::ChildCommandOf<AppCmd>, public RecordingCommandMixin {
public:
  explicit UserCmd(AppCmd& parent);
  pep::commandline::Parameters getSupportedParameters() const override;
  int execute() override;

  pep::commandline::Command* getCommand() override { return this; }
  AppCmd& getStorage() override;
  RecordingCommandMixin* getParentMixin() override;
};

class DatabaseCmd : public pep::commandline::ChildCommandOf<AppCmd>, public RecordingCommandMixin {
public:
  explicit DatabaseCmd(AppCmd& parent);
  pep::commandline::Parameters getSupportedParameters() const override;
  int execute() override;

  pep::commandline::Command* getCommand() override { return this; }
  AppCmd& getStorage() override;
  RecordingCommandMixin* getParentMixin() override;
};

class DeployCmd : public pep::commandline::DeprecatedChildCommandOf<AppCmd>, public RecordingCommandMixin {
public:
  explicit DeployCmd(AppCmd& parent);
  pep::commandline::Parameters getSupportedParameters() const override;
  int execute() override;

  pep::commandline::Command* getCommand() override { return this; }
  AppCmd& getStorage() override;
  RecordingCommandMixin* getParentMixin() override;
};

class StartCmd : public pep::commandline::ChildCommandOf<ServerCmd>, public RecordingCommandMixin {
public:
  explicit StartCmd(ServerCmd& parent);
  pep::commandline::Parameters getSupportedParameters() const override;
  int execute() override;

  pep::commandline::Command* getCommand() override { return this; }
  AppCmd& getStorage() override;
  RecordingCommandMixin* getParentMixin() override;
};

class ServerCmd : public pep::commandline::ChildCommandOf<AppCmd>, public RecordingCommandMixin {
public:
  explicit ServerCmd(AppCmd& parent);
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;

  pep::commandline::Command* getCommand() override { return this; }
  AppCmd& getStorage() override;
  RecordingCommandMixin* getParentMixin() override;
};

class AppCmd : public pep::commandline::Command, public RecordingCommandMixin {
public:
  std::string getName() const override;
  std::string getDescription() const override;
  pep::commandline::Parameters getSupportedParameters() const override;
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;

  void recordCommandValues(const pep::commandline::CommandPath& commandPath, const pep::commandline::NamedValues& values);

  pep::commandline::Command* getCommand() override { return this; }
  AppCmd& getStorage() override { return *this; }

  // Convenience methods for test assertions (inline implementations)
  template <typename T>
  std::optional<T> getCapturedValue(const pep::commandline::CommandPath& commandPath, const std::string& paramName) const {
    auto commandIt = mCapturedParams.find(commandPath.toString());
    if (commandIt == mCapturedParams.end()) {
      return std::nullopt;
    }
    return commandIt->second.getOptional<T>(paramName);
  }

  template <typename T>
  std::vector<T> getCapturedValues(const pep::commandline::CommandPath& commandPath, const std::string& paramName) const {
    auto commandIt = mCapturedParams.find(commandPath.toString());
    if (commandIt == mCapturedParams.end()) {
      return {};
    }
    return commandIt->second.getOptionalMultiple<T>(paramName);
  }

  bool hasCapturedParam(const pep::commandline::CommandPath& commandPath, const std::string& paramName) const {
    auto commandIt = mCapturedParams.find(commandPath.toString());
    if (commandIt == mCapturedParams.end()) {
      return false;
    }
    return commandIt->second.has(paramName);
  }

private:
  std::map<std::string, pep::commandline::NamedValues> mCapturedParams;
};

} // namespace pep::application::test
