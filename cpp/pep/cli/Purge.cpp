#include <pep/cli/CliApplication.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/client/Client.hpp>
#include <pep/storagefacility/Constants.hpp>

#include <rxcpp/operators/rx-tap.hpp>

namespace pep::cli {

namespace {

constexpr const char* DestructiveWarning = "Falsifies repository history, making it impossible to re-download exact copies of data sets that include purged items.";

class CommandPurge : public ChildCommandOf<CliApplication> {
public:
  explicit CommandPurge(CliApplication& parent);

protected:
  std::vector<std::shared_ptr<commandline::Command>> createChildCommands() override;
  std::optional<std::string> getAdditionalDescription() const override;

private:
  class BackCatalog;
  class Cancel;
};

class CommandPurge::BackCatalog : public ChildCommandOf<CommandPurge> {
public:
  explicit BackCatalog(CommandPurge& parent);
  std::optional<std::string> getAdditionalDescription() const override;

protected:
  int execute() override;
};

class CommandPurge::Cancel : public ChildCommandOf<CommandPurge> {
public:
  explicit Cancel(CommandPurge& parent);
  commandline::Parameters getSupportedParameters() const override;

protected:
  int execute() override;
};

CommandPurge::CommandPurge(CliApplication& parent)
  : ChildCommandOf<CliApplication>("purge", "Remove entries from the repository history", parent) {
}

std::vector<std::shared_ptr<commandline::Command>> CommandPurge::createChildCommands() {
  std::vector<std::shared_ptr<commandline::Command>> result;
  result.emplace_back(std::make_shared<BackCatalog>(*this));
  result.emplace_back(std::make_shared<Cancel>(*this));
  return result;
}

std::optional<std::string> CommandPurge::getAdditionalDescription() const {
  return DestructiveWarning;
}

CommandPurge::BackCatalog::BackCatalog(CommandPurge& parent)
  : ChildCommandOf<CommandPurge>("back-catalog", "Start a purge cycle that discards the repository's cell histories: only the current (rolling) data set will remain", parent) {
}

std::optional<std::string> CommandPurge::BackCatalog::getAdditionalDescription() const {
  return DestructiveWarning;
}

int CommandPurge::BackCatalog::execute() {
  return this->executeEventLoopForClient(true, [](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
    return client->getStorageFacilityProxy()->createPurgeCycle(PurgeCycle::Definition(BackCatalogPurgeFilter))
      .map([](PurgeCycle::Id id) {
          std::cout << "Purge cycle ID " << id;
          return FakeVoid();
        });
    });
}

CommandPurge::Cancel::Cancel(CommandPurge& parent)
  : ChildCommandOf<CommandPurge>("cancel", "Cancel a purge cycle", parent) {
}

commandline::Parameters CommandPurge::Cancel::getSupportedParameters() const {
  return ChildCommandOf<CommandPurge>::getSupportedParameters()
    + commandline::Parameter("id", "Purge cycle identifier").value(commandline::Value<PurgeCycle::Id>().positional().required());
}

int CommandPurge::Cancel::execute() {
  auto id = this->getParameterValues().get<PurgeCycle::Id>("id");
  return this->executeEventLoopForClient(true, [id](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
    return client->getStorageFacilityProxy()->cancelPurgeCycle(id);
    });
}

} // End anonymous namespace

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandPurge(CliApplication& parent) {
  return std::make_shared<CommandPurge>(parent);
}

}
