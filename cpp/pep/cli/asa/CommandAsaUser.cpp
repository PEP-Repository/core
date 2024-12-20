#include <pep/cli/asa/CommandAsaUser.hpp>

#include <rxcpp/operators/rx-map.hpp>

class CommandAsa::CommandAsaUser::AsaUserSubCommand : public ChildCommandOf<CommandAsa::CommandAsaUser> {
public:
  typedef rxcpp::observable<pep::FakeVoid>(pep::Client::*ClientMethod)(std::string);

private:
  ClientMethod mMethod;

public:
  AsaUserSubCommand(const std::string& name,
                    const std::string& description,
                    ClientMethod method,
                    CommandAsaUser& parent)
    : ChildCommandOf<CommandAsaUser>(name, description, parent), mMethod(method) {
  }

protected:
  virtual pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandAsaUser>::getSupportedParameters()
      + pep::commandline::Parameter("uid", "User identifier").value(pep::commandline::Value<std::string>().positional().required());
  }

  virtual int execute() override {
    return this->executeEventLoopFor([this](std::shared_ptr<pep::Client> client) {
      return ((*client).*mMethod)(this->getParameterValues().get<std::string>("uid"));
      });
  }
};

class CommandAsa::CommandAsaUser::AsaUserAddIdentifierSubCommand : public ChildCommandOf<CommandAsa::CommandAsaUser> {
public:
  AsaUserAddIdentifierSubCommand(CommandAsaUser& parent)
    : ChildCommandOf<CommandAsaUser>("addIdentifier", "Add identifier for a user", parent) {
  }

protected:
  virtual pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandAsaUser>::getSupportedParameters()
      + pep::commandline::Parameter("existingUid", "Existing user identifier").value(pep::commandline::Value<std::string>().positional().required())
      + pep::commandline::Parameter("newUid", "New user identifier to add").value(pep::commandline::Value<std::string>().positional().required());
  }

  virtual int execute() override {
    return this->executeEventLoopFor([this](std::shared_ptr<pep::Client> client) {
      return client->asaAddUserIdentifier(this->getParameterValues().get<std::string>("existingUid"), this->getParameterValues().get<std::string>("newUid"));
    });
  }
};

class CommandAsa::CommandAsaUser::AsaUserGroupUserSubCommand : public ChildCommandOf<CommandAsa::CommandAsaUser> {
public:
  typedef rxcpp::observable<pep::FakeVoid> (pep::Client::*ClientMethod)(std::string, std::string);

private:
  ClientMethod mMethod;

public:
  AsaUserGroupUserSubCommand(const std::string& name,
                    const std::string& description,
                    ClientMethod method,
                    CommandAsaUser& parent)
    : ChildCommandOf<CommandAsaUser>(name, description, parent),
      mMethod(method) {}

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandAsaUser>::getSupportedParameters()
         + pep::commandline::Parameter("uid", "User identifier")
               .value(pep::commandline::Value<std::string>().positional().required())
         + pep::commandline::Parameter("group", "Name of user group")
               .value(pep::commandline::Value<std::string>().positional().required());
  }

  int execute() override {
    return this->executeEventLoopFor([this](std::shared_ptr<pep::Client> client) {
      return ((*client).*mMethod)(this->getParameterValues().get<std::string>("uid"),
                                  this->getParameterValues().get<std::string>("group"));
    });
  }
};

std::vector<std::shared_ptr<pep::commandline::Command>> CommandAsa::CommandAsaUser::createChildCommands() {
  return {
      std::make_shared<AsaUserSubCommand>("create", "Create a new user", &pep::Client::asaCreateUser, *this),
      std::make_shared<AsaUserSubCommand>("remove", "Remove a user", &pep::Client::asaRemoveUser, *this),
      std::make_shared<AsaUserAddIdentifierSubCommand>(*this),
      std::make_shared<AsaUserSubCommand>("removeIdentifier", "Remove identifier for a user", &pep::Client::asaRemoveUserIdentifier, *this),
      std::make_shared<AsaUserGroupUserSubCommand>("addTo", "Add user to a group", &pep::Client::asaAddUserToGroup, *this),
      std::make_shared<AsaUserGroupUserSubCommand>("removeFrom",
                                          "Remove user from a group",
                                          &pep::Client::asaRemoveUserFromGroup,
                                          *this),
  };
}
