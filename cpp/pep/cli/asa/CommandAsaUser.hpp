#pragma once

#include <pep/cli/Asa.hpp>

class CommandAsa::CommandAsaUser : public ChildCommandOf<CommandAsa> {
public:
  explicit CommandAsaUser(CommandAsa& parent)
    : ChildCommandOf<CommandAsa>("user", "Manage user membership of groups", parent) {}

private:
  class AsaUserSubCommand;
  class AsaUserAddIdentifierSubCommand;
  class AsaUserGroupUserSubCommand;

protected:
  virtual std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands();
};
