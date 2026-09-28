#include <pep/cli/CliApplication.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/client/Client.hpp>
#include <pep/storagefacility/Constants.hpp>

#include <rxcpp/operators/rx-tap.hpp>

namespace pep::cli {

namespace {

constexpr const char* DestructiveWarning = "Falsifies repository history, making it impossible to re-download exact copies of data sets that include purged items.";

void OutputPurgeCycle(const PurgeCycle& cycle) {
  std::cout
    << "Purge cycle " << cycle.id
    << " for " << cycle.definition.name()
    << " announced at " << TimestampToXmlDateTime(cycle.announced);
  if (cycle.applied) {
    std::cout << "; applied at " << TimestampToXmlDateTime(*cycle.applied);
  }
  if (cycle.completed) {
    assert(cycle.applied);
    std::cout << "; completed at " << TimestampToXmlDateTime(*cycle.completed);
  }

  std::cout << '\n';
}

class CommandPurge : public ChildCommandOf<CliApplication> {
public:
  explicit CommandPurge(CliApplication& parent);

protected:
  std::vector<std::shared_ptr<commandline::Command>> createChildCommands() override;
  std::optional<std::string> getAdditionalDescription() const override;

private:
  class List;
  class BackCatalog;
  class Cancel;
  class Confirm;
};

class CommandPurge::List : public ChildCommandOf<CommandPurge> {
public:
  explicit List(CommandPurge& parent);

protected:
  int execute() override;
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

class CommandPurge::Confirm : public ChildCommandOf<CommandPurge> {
public:
  explicit Confirm(CommandPurge& parent);
  commandline::Parameters getSupportedParameters() const override;

protected:
  int execute() override;
};

CommandPurge::CommandPurge(CliApplication& parent)
  : ChildCommandOf<CliApplication>("purge", "Remove entries from the repository history", parent) {
}

std::vector<std::shared_ptr<commandline::Command>> CommandPurge::createChildCommands() {
  std::vector<std::shared_ptr<commandline::Command>> result;
  result.emplace_back(std::make_shared<List>(*this));
  result.emplace_back(std::make_shared<BackCatalog>(*this));
  result.emplace_back(std::make_shared<Cancel>(*this));
  result.emplace_back(std::make_shared<Confirm>(*this));
  return result;
}

CommandPurge::List::List(CommandPurge& parent)
  : ChildCommandOf<CommandPurge>("list", "List purge cycles", parent) {}

int CommandPurge::List::execute() {
  return this->executeEventLoopForClient(true, [](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
    return client->getStorageFacilityProxy()->listPurgeCycles()
      .map([](PurgeCycle cycle) {
          OutputPurgeCycle(cycle);
          return FakeVoid();
        });
    });
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
      .map([](PurgeCycle cycle) {
          OutputPurgeCycle(cycle);
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
      return client->getStorageFacilityProxy()->cancelPurgeCycle(id)
        .map([](PurgeCycle) {return FakeVoid(); });
    });
}

CommandPurge::Confirm::Confirm(CommandPurge& parent)
  : ChildCommandOf<CommandPurge>("confirm", "Confirm and start a purge cycle", parent) {}

commandline::Parameters CommandPurge::Confirm::getSupportedParameters() const {
  return ChildCommandOf<CommandPurge>::getSupportedParameters()
    + commandline::Parameter("id", "Purge cycle identifier").value(commandline::Value<PurgeCycle::Id>().positional().required());
}

int CommandPurge::Confirm::execute() {
  auto id = this->getParameterValues().get<PurgeCycle::Id>("id");
  return this->executeEventLoopForClient(true, [id](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
    return client->getStorageFacilityProxy()->confirmPurgeCycle(id)
      .map([](PurgeCycle cycle) {
          assert(cycle.applied.has_value());
          std::cout << "Purge cycle " << cycle.id
            << " for " << cycle.definition.name()
            << " applied at " << cycle.applied->time_since_epoch().count()
            << ", i.e. " << TimestampToXmlDateTime(*cycle.applied)
            << std::endl;
          return FakeVoid();
        });
    });
}

} // End anonymous namespace

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandPurge(CliApplication& parent) {
  return std::make_shared<CommandPurge>(parent);
}

}
