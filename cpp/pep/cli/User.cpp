#include <pep/application/Application.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/cli/user/CommandUserQuery.hpp>
#include <pep/cli/user/CommandUserGroup.hpp>
#include <pep/cli/user/UserSubCommands.hpp>
#include <pep/core-client/CoreClient.hpp>

using namespace pep::cli;

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandUser(CliApplication& parent) {
  return std::make_shared<CommandUser>(parent);
}

std::vector<std::shared_ptr<pep::commandline::Command>> CommandUser::createChildCommands() {
  return {
    std::make_shared<CommandUserQuery>(*this),
    std::make_shared<CommandUserGroup>(*this),
    std::make_shared<UserSubCommand>("create", "Create a new user", &pep::CoreClient::createUser, *this),
    std::make_shared<UserSubCommand>("remove", "Remove a user", &pep::CoreClient::removeUser, *this),
    std::make_shared<UserSubCommand>("setDisplayIdentifier", "Set the display identifier for user", &pep::CoreClient::setUserDisplayIdentifier, *this),
    std::make_shared<UserAddIdentifierSubCommand>(*this),
    std::make_shared<UserSubCommand>("removeIdentifier", "Remove identifier for a user", &pep::CoreClient::removeUserIdentifier, *this),
    std::make_shared<UserAddToSubCommand>(*this),
    std::make_shared<UserRemoveFromSubCommand>(*this),
  };
}

