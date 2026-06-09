#include <pep/application/Application.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/cli/user/CommandUserQuery.hpp>
#include <pep/cli/user/CommandUserGroup.hpp>
#include <pep/cli/user/UserSubCommands.hpp>

using namespace pep::cli;

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandUser(CliApplication& parent) {
  return std::make_shared<CommandUser>(parent);
}

std::vector<std::shared_ptr<pep::commandline::Command>> CommandUser::createChildCommands() {
  return {
    std::make_shared<CommandUserQuery>(*this),
    std::make_shared<CommandUserGroup>(*this),
    std::make_shared<UserSubCommand>("create", "Create a new user", &pep::AccessManagerProxy::createUser, *this),
    std::make_shared<UserSubCommand>("remove", "Remove a user", &pep::AccessManagerProxy::removeUser, *this),
    std::make_shared<UserSubCommand>("setDisplayId", "Set the display identifier for user", &pep::AccessManagerProxy::setUserDisplayId, *this),
    std::make_shared<UserSubCommand>("setPrimaryId", "Set the primary identifier for user", &pep::AccessManagerProxy::setUserPrimaryId, *this),
    std::make_shared<UserSubCommand>("unsetPrimaryId", "Set the primary identifier for user", &pep::AccessManagerProxy::unsetUserPrimaryId, *this),
    std::make_shared<UserAddIdentifierSubCommand>(*this),
    std::make_shared<UserSubCommand>("removeIdentifier", "Remove identifier for a user", &pep::AccessManagerProxy::removeUserIdentifier, *this),
    std::make_shared<UserAddToSubCommand>(*this),
    std::make_shared<UserRemoveFromSubCommand>(*this),
  };
}

