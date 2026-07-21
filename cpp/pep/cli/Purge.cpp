#include <pep/cli/CliApplication.hpp>
#include <pep/cli/Commands.hpp>

namespace pep::cli {

namespace {

class CommandPurge : public ChildCommandOf<CliApplication> {
public:
  explicit CommandPurge(CliApplication& parent);

protected:
  std::vector<std::shared_ptr<commandline::Command>> createChildCommands() override;
  std::optional<std::string> getAdditionalDescription() const override;

private:
  class Define;
  class Cancel;
};

class CommandPurge::Define : public ChildCommandOf<CommandPurge> {
public:
  explicit Define(CommandPurge& parent);

protected:
  int execute() override { throw std::runtime_error("This command has not been implemented yet"); }
};

class CommandPurge::Cancel : public ChildCommandOf<CommandPurge> {
public:
  explicit Cancel(CommandPurge& parent);

protected:
  int execute() override { throw std::runtime_error("This command has not been implemented yet"); }
};

CommandPurge::CommandPurge(CliApplication& parent)
  : ChildCommandOf<CliApplication>("purge", "Remove entries from the repository history", parent) {
}

std::vector<std::shared_ptr<pep::commandline::Command>> CommandPurge::createChildCommands() {
  std::vector<std::shared_ptr<pep::commandline::Command>> result;
  result.emplace_back(std::make_shared<Define>(*this));
  result.emplace_back(std::make_shared<Cancel>(*this));
  return result;
}

std::optional<std::string> CommandPurge::getAdditionalDescription() const {
  return "Falsifies repository history, making it impossible to re-download exact copies of (some) older data sets.";
}

CommandPurge::Define::Define(CommandPurge& parent)
  : ChildCommandOf<CommandPurge>("define", "Start a new purge cycle", parent) {
}

CommandPurge::Cancel::Cancel(CommandPurge& parent)
  : ChildCommandOf<CommandPurge>("cancel", "Cancel a purge cycle", parent) {
}

} // End anonymous namespace

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandPurge(CliApplication& parent) {
  return std::make_shared<CommandPurge>(parent);
}

}
