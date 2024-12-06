#include <pep/application/Application.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/asa/CommandAsaQuery.hpp>
#include <pep/cli/asa/CommandAsaToken.hpp>
#include <pep/cli/asa/CommandAsaUser.hpp>
#include <pep/cli/asa/CommandAsaUserGroup.hpp>

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandAsa(CliApplication& parent) {
  return std::make_shared<CommandAsa>(parent);
}

std::vector<std::shared_ptr<pep::commandline::Command>> CommandAsa::createChildCommands() {
  return {
      std::make_shared<CommandAsaQuery>(*this),
      std::make_shared<CommandAsaUserGroup>(*this),
      std::make_shared<CommandAsaUser>(*this),
      std::make_shared<CommandAsaToken>(*this),
  };
}
