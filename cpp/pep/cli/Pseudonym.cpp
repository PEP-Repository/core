#include <pep/application/Application.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/cli/Pseudonym.hpp>
#include <pep/cli/pseudonym/CommandConvert.hpp>

namespace pep::cli {

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandPseudonym(CliApplication &parent) {
  return std::make_shared<CommandPseudonym>(parent);
}

std::vector<std::shared_ptr<commandline::Command>> CommandPseudonym::createChildCommands() {
  return {std::make_shared<CommandPseudonym::CommandConvert>(*this)};
}

} // namespace pep::cli
