#pragma once

#include <pep/cli/Asa.hpp>

class CommandAsa::CommandAsaUserGroup : public ChildCommandOf<CommandAsa> {
public:
  explicit CommandAsaUserGroup(CommandAsa&);

private:
  class AsaUserGroupSubCommand;
  class AsaUserGroupRemoveCommand;

protected:
  virtual std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands();
};
