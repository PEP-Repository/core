#pragma once

#include <pep/application/CommandLineCommand.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/User.hpp>

#include <memory>
#include <vector>

class pep::cli::CommandUser::CommandUserGroup : public ChildCommandOf<CommandUser> {
public:
  explicit CommandUserGroup(CommandUser&);

private:
  class UserGroupSubCommand;
  class UserGroupRemoveCommand;

protected:
  std::vector<std::shared_ptr<Command>> createChildCommands() override;
};
