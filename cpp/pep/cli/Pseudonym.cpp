#include <pep/cli/Commands.hpp>
#include <pep/cli/Pseudonym.hpp>
#include <pep/cli/pseudonym/CommandConvert.hpp>
#include <pep/application/CommandLineCommand.hpp>
#include <pep/cli/CliApplication.hpp>
#include <pep/cli/Command.hpp>

#include <memory>
#include <vector>

namespace pep::cli {

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandPseudonym(CliApplication &parent) {
  return std::make_shared<CommandPseudonym>(parent);
}

std::vector<std::shared_ptr<commandline::Command>> CommandPseudonym::createChildCommands() {
  return {std::make_shared<CommandPseudonym::CommandConvert>(*this)};
}

} // namespace pep::cli
