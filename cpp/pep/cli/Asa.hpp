#pragma once

#include <pep/application/Application.hpp>
#include <pep/cli/Command.hpp>

class CommandAsa : public ChildCommandOf<CliApplication> {
public:
  explicit CommandAsa(CliApplication& parent)
    : ChildCommandOf<CliApplication>("asa", "Administer authserver", parent) {}

private:
  class CommandAsaUserGroup;
  class CommandAsaUser;
  class CommandAsaQuery;
  class CommandAsaToken;

protected:
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;
};
