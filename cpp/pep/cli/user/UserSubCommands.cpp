#include <pep/cli/user/UserSubCommands.hpp>
#include <pep/core-client/CoreClient.hpp>

using namespace pep::cli;

CommandUser::UserSubCommand::UserSubCommand(const std::string &name, const std::string &description,
                                            CommandUser::UserSubCommand::ClientMethod method, CommandUser &parent)
        : ChildCommandOf<CommandUser>(name, description, parent), mMethod(method) {
}

pep::commandline::Parameters CommandUser::UserSubCommand::getSupportedParameters() const {
  return ChildCommandOf<CommandUser>::getSupportedParameters()
         + pep::commandline::Parameter("uid", "User identifier").value(pep::commandline::Value<std::string>().positional().required());
}

int CommandUser::UserSubCommand::execute() {
  return this->executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
    return ((*client).*mMethod)(this->getParameterValues().get<std::string>("uid"));
  });
}

CommandUser::UserAddIdentifierSubCommand::UserAddIdentifierSubCommand(CommandUser &parent)
        : ChildCommandOf<CommandUser>("addIdentifier", "Add identifier for a user", parent) {
}

pep::commandline::Parameters CommandUser::UserAddIdentifierSubCommand::getSupportedParameters() const {
  return ChildCommandOf<CommandUser>::getSupportedParameters()
         + pep::commandline::Parameter("existingUid", "Existing user identifier").value(pep::commandline::Value<std::string>().positional().required())
         + pep::commandline::Parameter("newUid", "New user identifier to add").value(pep::commandline::Value<std::string>().positional().required())
         + pep::commandline::Parameter("displayId", "Whether the new user identifier should be the display identifier for the user.");
}

int CommandUser::UserAddIdentifierSubCommand::execute() {
  return this->executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
    return client->addUserIdentifier(this->getParameterValues().get<std::string>("existingUid"), this->getParameterValues().get<std::string>("newUid"), this->getParameterValues().has("displayId"));
  });
}

CommandUser::UserGroupUserSubCommand::UserGroupUserSubCommand(const std::string &name, const std::string &description, CommandUser &parent)
        : ChildCommandOf<CommandUser>(name, description, parent) {}

pep::commandline::Parameters CommandUser::UserGroupUserSubCommand::getSupportedParameters() const {
  return ChildCommandOf<CommandUser>::getSupportedParameters()
         + pep::commandline::Parameter("uid", "User identifier")
                 .value(pep::commandline::Value<std::string>().positional().required())
         + pep::commandline::Parameter("group", "Name of user group")
                 .value(pep::commandline::Value<std::string>().positional().required());
}

int CommandUser::UserAddToSubCommand::execute() {
  return this->executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
    return client->addUserToGroup(this->getParameterValues().get<std::string>("uid"),
                                this->getParameterValues().get<std::string>("group"));
  });
}

pep::commandline::Parameters CommandUser::UserRemoveFromSubCommand::getSupportedParameters() const {
  return UserGroupUserSubCommand::getSupportedParameters()
         + pep::commandline::Parameter("dontBlockTokens", "Don't block tokens for the user and user group.");
}

int CommandUser::UserRemoveFromSubCommand::execute() {
  return this->executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
    return client->removeUserFromGroup(this->getParameterValues().get<std::string>("uid"),
                                this->getParameterValues().get<std::string>("group"),
                                !this->getParameterValues().has("dontBlockTokens"));
  });
}
