#pragma once

#include <pep/accessmanager/AccessManagerProxy.hpp>
#include <pep/cli/User.hpp>

#include <rxcpp/operators/rx-map.hpp>

namespace pep::cli {
class CommandUser::UserSubCommand : public ChildCommandOf<CommandUser> {
public:
  using AmProxyMethod = rxcpp::observable<pep::FakeVoid>(pep::AccessManagerProxy::*)(std::string) const;

private:
  AmProxyMethod mMethod;

public:
  UserSubCommand(const std::string& name,
                    const std::string& description,
                    AmProxyMethod method,
                    CommandUser& parent);

protected:
  pep::commandline::Parameters getSupportedParameters() const override;

  int execute() override;
};

class CommandUser::UserAddIdentifierSubCommand : public ChildCommandOf<CommandUser> {
public:
  UserAddIdentifierSubCommand(CommandUser& parent);

protected:
  pep::commandline::Parameters getSupportedParameters() const override;

  int execute() override;
};

class CommandUser::UserGroupUserSubCommand : public ChildCommandOf<CommandUser> {
public:
  UserGroupUserSubCommand(const std::string& name,
                    const std::string& description,
                    CommandUser& parent);

protected:
  pep::commandline::Parameters getSupportedParameters() const override;

  int execute() override = 0;
};

class CommandUser::UserAddToSubCommand : public CommandUser::UserGroupUserSubCommand {
public:
  UserAddToSubCommand(CommandUser& parent) : CommandUser::UserGroupUserSubCommand("addTo", "Add user to a group", parent) {};

protected:
  int execute() override;
};

class CommandUser::UserRemoveFromSubCommand : public CommandUser::UserGroupUserSubCommand {
public:
  UserRemoveFromSubCommand(CommandUser& parent) : CommandUser::UserGroupUserSubCommand("removeFrom", "Remove user from a group", parent) {};

protected:
  pep::commandline::Parameters getSupportedParameters() const override;

  int execute() override;
};
}
