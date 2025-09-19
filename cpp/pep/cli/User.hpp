#pragma once

#include <pep/application/Application.hpp>
#include <pep/cli/Command.hpp>

namespace pep::cli {

class CommandUser : public ChildCommandOf<CliApplication> {
public:
  explicit CommandUser(CliApplication& parent)
    : ChildCommandOf<CliApplication>("user", "Administer users", parent) {}

private:
  class CommandUserGroup;
  class UserSubCommand;
  class UserAddIdentifierSubCommand;
  class UserGroupUserSubCommand;
  class UserAddToSubCommand;
  class UserRemoveFromSubCommand;
  class CommandUserQuery;

protected:
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;
};

}
