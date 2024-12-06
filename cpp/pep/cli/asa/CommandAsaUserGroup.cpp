#include <pep/cli/asa/CommandAsaUserGroup.hpp>

#include <rxcpp/operators/rx-map.hpp>

CommandAsa::CommandAsaUserGroup::CommandAsaUserGroup(CommandAsa& parent)
  : ChildCommandOf<CommandAsa>("group", "Manage user groups", parent) {}

class CommandAsa::CommandAsaUserGroup::AsaUserGroupSubCommand : public ChildCommandOf<CommandAsa::CommandAsaUserGroup> {
public:
  using ClientMethod = rxcpp::observable<pep::FakeVoid> (pep::Client::*)(std::string, pep::UserGroupProperties);

private:
  ClientMethod mMethod;

public:
  AsaUserGroupSubCommand(const std::string& name,
                         const std::string& description,
                         ClientMethod method,
                         CommandAsaUserGroup& parent)
    : ChildCommandOf<CommandAsaUserGroup>(name, description, parent),
      mMethod(method) {}

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandAsaUserGroup>::getSupportedParameters()
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
    return this->executeEventLoopFor([this](std::shared_ptr<pep::Client> client) {
      pep::UserGroupProperties properties;
      properties.mMaxAuthValidity = this->getParameterValues().getOptional<std::chrono::seconds>("max-auth-validity");
      return ((*client).*mMethod)(this->getParameterValues().get<std::string>("name"), properties);
    });
  }
};

class CommandAsa::CommandAsaUserGroup::AsaUserGroupRemoveCommand
  : public ChildCommandOf<CommandAsa::CommandAsaUserGroup> {
public:
  AsaUserGroupRemoveCommand(CommandAsaUserGroup& parent)
    : ChildCommandOf<CommandAsaUserGroup>("remove", "Remove user group", parent) {}

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandAsaUserGroup>::getSupportedParameters()
         + pep::commandline::Parameter("name", "Name of user group")
               .value(pep::commandline::Value<std::string>().positional().required());
  }

  int execute() override {
    return this->executeEventLoopFor([this](std::shared_ptr<pep::Client> client) {
      return client->asaRemoveUserGroup(this->getParameterValues().get<std::string>("name"));
    });
  }
};

std::vector<std::shared_ptr<pep::commandline::Command>> CommandAsa::CommandAsaUserGroup::createChildCommands() {
  return {
      std::make_shared<AsaUserGroupSubCommand>("create",
                                               "Create new user group",
                                               &pep::Client::asaCreateUserGroup,
                                               *this),
      std::make_shared<AsaUserGroupSubCommand>("modify", "Modify user group", &pep::Client::asaModifyUserGroup, *this),
      std::make_shared<AsaUserGroupRemoveCommand>(*this),
  };
}
