#pragma once

#include <pep/cli/User.hpp>
#include <pep/core-client/CoreClient_fwd.hpp>

#include <rxcpp/operators/rx-map.hpp>

namespace pep::cli {
class CommandUser::UserSubCommand : public ChildCommandOf<CommandUser> {
public:
  using ClientMethod = rxcpp::observable<pep::FakeVoid>(pep::CoreClient::*)(std::string);

private:
  ClientMethod mMethod;

public:
  UserSubCommand(const std::string& name,
                    const std::string& description,
                    ClientMethod method,
                    CommandUser& parent);

protected:
  virtual pep::commandline::Parameters getSupportedParameters() const override;

  virtual int execute() override;
};

class CommandUser::UserAddIdentifierSubCommand : public ChildCommandOf<CommandUser> {
public:
  UserAddIdentifierSubCommand(CommandUser& parent);

protected:
  virtual pep::commandline::Parameters getSupportedParameters() const override;

  virtual int execute() override;
};

class CommandUser::UserGroupUserSubCommand : public ChildCommandOf<CommandUser> {
public:
  using ClientMethod = rxcpp::observable<pep::FakeVoid> (pep::CoreClient::*)(std::string, std::string);

private:
  ClientMethod mMethod;

public:
  UserGroupUserSubCommand(const std::string& name,
                    const std::string& description,
                    ClientMethod method,
                    CommandUser& parent);

protected:
  pep::commandline::Parameters getSupportedParameters() const override;

  int execute() override;
};
}
