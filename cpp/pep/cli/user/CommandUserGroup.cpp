#include <pep/cli/user/CommandUserGroup.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <rxcpp/operators/rx-map.hpp>

using namespace pep::cli;

CommandUser::CommandUserGroup::CommandUserGroup(CommandUser& parent)
  : ChildCommandOf<CommandUser>("group", "Manage user groups", parent) {}

class CommandUser::CommandUserGroup::UserGroupSubCommand : public ChildCommandOf<CommandUser::CommandUserGroup> {
public:
  using ClientMethod = rxcpp::observable<pep::FakeVoid> (pep::CoreClient::*)(pep::UserGroup);

private:
  ClientMethod mMethod;

public:
  UserGroupSubCommand(const std::string& name,
                      const std::string& description,
                      ClientMethod method,
                      CommandUserGroup& parent)
    : ChildCommandOf<CommandUserGroup>(name, description, parent),
      mMethod(method) {}

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandUserGroup>::getSupportedParameters()
         + pep::commandline::Parameter("name", "Name of user group")
               .value(pep::commandline::Value<std::string>().positional().required())
         + pep::commandline::Parameter(
               "max-auth-validity",
               "Allow users in this group to request authentication for at most the specified period. Use suffix "
               "d/day(s), h/hour(s), m/minute(s) or s/second(s). "
               "Omit this parameter if users in this group should not be allowed to request long-lived authentication.")
               .value(pep::commandline::Value<std::chrono::seconds>());
  }

  int execute() override {
    return this->executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
      pep::UserGroup userGroup;
      userGroup.mMaxAuthValidity = this->getParameterValues().getOptional<std::chrono::seconds>("max-auth-validity");
      userGroup.mName = this->getParameterValues().get<std::string>("name");
      return ((*client).*mMethod)(userGroup);
    });
  }
};

class CommandUser::CommandUserGroup::UserGroupRemoveCommand
  : public ChildCommandOf<CommandUser::CommandUserGroup> {
public:
  UserGroupRemoveCommand(CommandUserGroup& parent)
    : ChildCommandOf<CommandUserGroup>("remove", "Remove user group", parent) {}

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandUserGroup>::getSupportedParameters()
         + pep::commandline::Parameter("name", "Name of user group")
               .value(pep::commandline::Value<std::string>().positional().required());
  }

  int execute() override {
    return this->executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
      return client->removeUserGroup(this->getParameterValues().get<std::string>("name"));
    });
  }
};

std::vector<std::shared_ptr<pep::commandline::Command>> CommandUser::CommandUserGroup::createChildCommands() {
  return {
    std::make_shared<UserGroupSubCommand>("create",
                                          "Create new user group",
                                          &pep::CoreClient::createUserGroup,
                                          *this),
    std::make_shared<UserGroupSubCommand>("modify", "Modify user group", &pep::CoreClient::modifyUserGroup, *this),
    std::make_shared<UserGroupRemoveCommand>(*this),
  };
}
