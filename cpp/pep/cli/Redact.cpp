#include <pep/application/CommandLineCommand.hpp>
#include <pep/application/CommandLineParameter.hpp>
#include <pep/async/FakeVoid.hpp>
#include <pep/async/RxToVector.hpp>
#include <pep/cli/CliApplication.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/client/Client.hpp>
#include <pep/storagefacility/Constants.hpp>
#include <pep/storagefacility/RedactionCycle.hpp>
#include <pep/structuredoutput/FormatFlags.hpp>
#include <pep/structuredoutput/Json.hpp>
#include <pep/structuredoutput/Table.hpp>
#include <pep/structuredoutput/Yaml.hpp>
#include <pep/utils/Timestamp.hpp>

#include <rxcpp/operators/rx-tap.hpp>

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace pep::cli {

namespace {

constexpr const char* DestructiveWarning = "Redaction makes it impossible to (re-)download (exact copies of) data sets that include redacted cell versions.";

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

class CommandRedact : public ChildCommandOf<CliApplication> {
public:
  explicit CommandRedact(CliApplication& parent);

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

class CommandRedact::OutputtingChildCommand : public ChildCommandOf<CommandRedact> {
private:
  OutputFormat outputFormat() const;

protected:
  using ChildCommandOf<CommandRedact>::ChildCommandOf;

  commandline::Parameters getSupportedParameters() const override;
  void output(const std::vector<RedactionCycle>& cycles) const;
  void output(RedactionCycle cycle) const;
};

class CommandRedact::List : public OutputtingChildCommand {
public:
  explicit List(CommandRedact& parent);

protected:
  int execute() override;
};

class CommandRedact::BackCatalog : public OutputtingChildCommand {
public:
  explicit BackCatalog(CommandRedact& parent);
  std::optional<std::string> getAdditionalDescription() const override;

protected:
  int execute() override;
};

class CommandRedact::Cancel : public ChildCommandOf<CommandRedact> {
public:
  explicit Cancel(CommandRedact& parent);
  commandline::Parameters getSupportedParameters() const override;

protected:
  int execute() override;
};

class CommandRedact::Confirm : public OutputtingChildCommand {
public:
  explicit Confirm(CommandRedact& parent);
  std::optional<std::string> getAdditionalDescription() const override;
  commandline::Parameters getSupportedParameters() const override;

protected:
  int execute() override;
};

CommandRedact::CommandRedact(CliApplication& parent)
  : ChildCommandOf<CliApplication>("redact", "Make repository history entries inaccessible", parent) {
}

std::vector<std::shared_ptr<commandline::Command>> CommandRedact::createChildCommands() {
  std::vector<std::shared_ptr<commandline::Command>> result;
  result.emplace_back(std::make_shared<List>(*this));
  result.emplace_back(std::make_shared<BackCatalog>(*this));
  result.emplace_back(std::make_shared<Cancel>(*this));
  result.emplace_back(std::make_shared<Confirm>(*this));
  return result;
}

commandline::Parameters CommandRedact::OutputtingChildCommand::getSupportedParameters() const {
  auto formatNames = OutputFormat::All() | std::views::transform(&OutputFormat::name) | std::ranges::to<std::vector>();
  constexpr auto defaultFormatName = "yaml";
  assert(std::ranges::contains(formatNames, defaultFormatName));

  return ChildCommandOf<CommandRedact>::getSupportedParameters()
    + commandline::Parameter("output-format", "The format in which to produce output").value(commandline::Value<std::string>().allow(std::vector<std::string>(formatNames)).defaultsTo(defaultFormatName));
}

void CommandRedact::OutputtingChildCommand::output(const std::vector<RedactionCycle>& cycles) const {
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

void CommandRedact::OutputtingChildCommand::output(RedactionCycle cycle) const {
  return this->output(std::vector<RedactionCycle>({ std::move(cycle) }));
}


CommandRedact::List::List(CommandRedact& parent)
  : OutputtingChildCommand("list", "List redaction cycles", parent) {}

int CommandRedact::List::execute() {
  return this->executeEventLoopForClient(true, [this](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
    return client->getStorageFacilityProxy()->listRedactionCycles()
      .op(pep::RxToVector())
      .map([this](std::shared_ptr<std::vector<RedactionCycle>> cycles) {
          this->output(*cycles);
          return FakeVoid();
        });
    });
}

std::optional<std::string> CommandRedact::getAdditionalDescription() const {
  return DestructiveWarning;
}

CommandRedact::BackCatalog::BackCatalog(CommandRedact& parent)
  : OutputtingChildCommand("back-catalog", "Start a redaction cycle that discards the repository's cell histories: only the current (rolling) data set will remain", parent) {
}

std::optional<std::string> CommandRedact::BackCatalog::getAdditionalDescription() const {
  return DestructiveWarning;
}

int CommandRedact::BackCatalog::execute() {
  return this->executeEventLoopForClient(true, [this](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
    return client->getStorageFacilityProxy()->announceRedactionCycle(RedactionCycle::Definition(BackCatalogRedactionFilter))
      .map([this](RedactionCycle cycle) {
          this->output(std::move(cycle));
          return FakeVoid();
        });
    });
}

CommandRedact::Cancel::Cancel(CommandRedact& parent)
  : ChildCommandOf<CommandRedact>("cancel", "Cancel a redaction cycle", parent) {
}

commandline::Parameters CommandRedact::Cancel::getSupportedParameters() const {
  return ChildCommandOf<CommandRedact>::getSupportedParameters()
    + commandline::Parameter("id", "Redaction cycle identifier").value(commandline::Value<RedactionCycle::Id>().positional().required());
}

int CommandRedact::Cancel::execute() {
  auto id = this->getParameterValues().get<RedactionCycle::Id>("id");
  return this->executeEventLoopForClient(true, [id](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
      return client->getStorageFacilityProxy()->cancelRedactionCycle(id)
        .map([](RedactionCycle) {return FakeVoid(); });
    });
}

CommandRedact::Confirm::Confirm(CommandRedact& parent)
  : OutputtingChildCommand("confirm", "Confirm and start a redaction cycle", parent) {}

std::optional<std::string> CommandRedact::Confirm::getAdditionalDescription() const {
  return DestructiveWarning;
}

commandline::Parameters CommandRedact::Confirm::getSupportedParameters() const {
  return OutputtingChildCommand::getSupportedParameters()
    + commandline::Parameter("id", "Redaction cycle identifier").value(commandline::Value<RedactionCycle::Id>().positional().required());
}

int CommandRedact::Confirm::execute() {
  auto id = this->getParameterValues().get<RedactionCycle::Id>("id");
  return this->executeEventLoopForClient(true, [this, id](std::shared_ptr<Client> client) -> rxcpp::observable<FakeVoid> {
    return client->getStorageFacilityProxy()->confirmRedactionCycle(id)
      .map([this](RedactionCycle cycle) {
          assert(cycle.applied.has_value());
          this->output(std::move(cycle));
          return FakeVoid();
        });
    });
}

} // End anonymous namespace

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandRedact(CliApplication& parent) {
  return std::make_shared<CommandRedact>(parent);
}

}
