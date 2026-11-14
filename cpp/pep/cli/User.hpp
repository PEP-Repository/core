#pragma once

#include <pep/application/CommandLineCommand.hpp>
#include <pep/cli/CliApplication.hpp>
#include <pep/cli/Command.hpp>

#include <memory>
#include <vector>

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
  class UserUpdateExpirationSubCommand;
  class CommandUserQuery;

protected:
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;
};

}
