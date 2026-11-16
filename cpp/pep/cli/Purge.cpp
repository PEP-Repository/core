#include <pep/async/RxToVector.hpp>
#include <pep/cli/CliApplication.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/client/Client.hpp>
#include <pep/storagefacility/Constants.hpp>
#include <pep/structuredoutput/FormatFlags.hpp>
#include <pep/structuredoutput/Json.hpp>
#include <pep/structuredoutput/Yaml.hpp>

#include <rxcpp/operators/rx-tap.hpp>

namespace pep::cli {

namespace {

constexpr const char* DestructiveWarning = "Purging makes it impossible to (re-)download (exact copies of) data sets that include purged cell versions.";

std::string TimestampToString(const std::optional<Timestamp> timestamp) {
  return timestamp.has_value()
    ? TimestampToXmlDateTime(*timestamp)
    : std::string();
}

namespace so = pep::structuredOutput;

class OutputFormat {
public:
  using TableOutputFunction = std::function<std::ostream& (std::ostream&, const so::Table&)>;

private:
  so::FormatFlags id_;
  TableOutputFunction outputTable_;

public:
  OutputFormat(so::FormatFlags id, TableOutputFunction outputTable) noexcept
    : id_(id), outputTable_(std::move(outputTable)) {
  }

  std::string name() const {
    return so::ToSingleString(id_);
  }

  std::ostream& operator()(std::ostream& destination, const so::Table& table) const {
    return outputTable_(destination, table);
  }

  static std::vector<OutputFormat> All();
};

std::vector<OutputFormat> OutputFormat::All() {
  return {
    OutputFormat(so::FormatFlags::Json, [](std::ostream& stream, const so::Table& table) -> std::ostream& { return so::json::append(stream, table); }),
    OutputFormat(so::FormatFlags::Yaml, [](std::ostream& stream, const so::Table& table) -> std::ostream& { return so::yaml::append(stream, table); }),
  };
}

class CommandPurge : public ChildCommandOf<CliApplication> {
public:
  explicit CommandPurge(CliApplication& parent);

protected:
  std::vector<std::shared_ptr<commandline::Command>> createChildCommands() override;
  std::optional<std::string> getAdditionalDescription() const override;

private:
  class OutputtingChildCommand;
  class List;
  class BackCatalog;
  class Cancel;
  class Confirm;
};

class CommandPurge::OutputtingChildCommand : public ChildCommandOf<CommandPurge> {
private:
  OutputFormat outputFormat() const;

protected:
  using ChildCommandOf<CommandPurge>::ChildCommandOf;

  commandline::Parameters getSupportedParameters() const override;
  void output(const std::vector<PurgeCycle>& cycles) const;
  void output(PurgeCycle cycle) const;
};

class CommandPurge::List : public OutputtingChildCommand {
public:
  explicit List(CommandPurge& parent);

protected:
  int execute() override;
};

class CommandPurge::BackCatalog : public OutputtingChildCommand {
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

class CommandPurge::Confirm : public OutputtingChildCommand {
public:
  explicit Confirm(CommandPurge& parent);
  std::optional<std::string> getAdditionalDescription() const override;
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

commandline::Parameters CommandPurge::OutputtingChildCommand::getSupportedParameters() const {
  auto formatNames = OutputFormat::All() | std::views::transform(&OutputFormat::name) | std::ranges::to<std::vector>();
  constexpr auto defaultFormatName = "yaml";
  assert(std::ranges::contains(formatNames, defaultFormatName));

  return ChildCommandOf<CommandPurge>::getSupportedParameters()
    + commandline::Parameter("output-format", "The format in which to produce output").value(commandline::Value<std::string>().allow(std::vector<std::string>(formatNames)).defaultsTo(defaultFormatName));
}

void CommandPurge::OutputtingChildCommand::output(const std::vector<PurgeCycle>& cycles) const {
  std::vector<std::string> data;
  for (const auto& cycle : cycles) {
    std::vector<std::string> single{
      std::to_string(cycle.id),
      cycle.definition.name(),
      TimestampToString(cycle.announced),
      TimestampToString(cycle.applied),
      TimestampToString(cycle.completed),
    };
    data.insert(data.end(), single.begin(), single.end());
  }

  auto table = so::Table::FromSeparateHeaderAndData(
    { "ID", "name", "announced", "applied", "completed" },
    data
  );

  auto formatName = this->getParameterValues().get<std::string>("output-format");
  auto formats = OutputFormat::All();
  auto position = std::ranges::find_if(formats, [&formatName](const OutputFormat& candidate) {return candidate.name() == formatName; });
  assert(position != formats.end());
  (*position)(std::cout, table);
}

void CommandPurge::OutputtingChildCommand::output(PurgeCycle cycle) const {
  return this->output(std::vector<PurgeCycle>({ std::move(cycle) }));
}


CommandPurge::List::List(CommandPurge& parent)
  : OutputtingChildCommand("list", "List purge cycles", parent) {}

int CommandPurge::List::execute() {
  return this->executeEventLoopForClient(true, [this](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
    return client->getStorageFacilityProxy()->listPurgeCycles()
      .op(pep::RxToVector())
      .map([this](std::shared_ptr<std::vector<PurgeCycle>> cycles) {
          this->output(*cycles);
          return FakeVoid();
        });
    });
}

std::optional<std::string> CommandPurge::getAdditionalDescription() const {
  return DestructiveWarning;
}

CommandPurge::BackCatalog::BackCatalog(CommandPurge& parent)
  : OutputtingChildCommand("back-catalog", "Start a purge cycle that discards the repository's cell histories: only the current (rolling) data set will remain", parent) {
}

std::optional<std::string> CommandPurge::BackCatalog::getAdditionalDescription() const {
  return DestructiveWarning;
}

int CommandPurge::BackCatalog::execute() {
  return this->executeEventLoopForClient(true, [this](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
    return client->getStorageFacilityProxy()->announcePurgeCycle(PurgeCycle::Definition(BackCatalogPurgeFilter))
      .map([this](PurgeCycle cycle) {
          this->output(std::move(cycle));
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
  : OutputtingChildCommand("confirm", "Confirm and start a purge cycle", parent) {}

std::optional<std::string> CommandPurge::Confirm::getAdditionalDescription() const {
  return DestructiveWarning;
}

commandline::Parameters CommandPurge::Confirm::getSupportedParameters() const {
  return OutputtingChildCommand::getSupportedParameters()
    + commandline::Parameter("id", "Purge cycle identifier").value(commandline::Value<PurgeCycle::Id>().positional().required());
}

int CommandPurge::Confirm::execute() {
  auto id = this->getParameterValues().get<PurgeCycle::Id>("id");
  return this->executeEventLoopForClient(true, [this, id](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
    return client->getStorageFacilityProxy()->confirmPurgeCycle(id)
      .map([this](PurgeCycle cycle) {
          assert(cycle.applied.has_value());
          this->output(std::move(cycle));
          return FakeVoid();
        });
    });
}

} // End anonymous namespace

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandPurge(CliApplication& parent) {
  return std::make_shared<CommandPurge>(parent);
}

}
