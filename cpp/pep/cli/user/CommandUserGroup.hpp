#pragma once

#include <pep/cli/User.hpp>

class pep::cli::CommandUser::CommandUserGroup : public ChildCommandOf<CommandUser> {
public:
  explicit CommandUserGroup(CommandUser&);

private:
  class UserGroupSubCommand;
  class UserGroupRemoveCommand;

protected:
  std::vector<std::shared_ptr<Command>> createChildCommands() override;
};
