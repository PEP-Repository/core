#pragma once

#include <pep/application/CommandLineCommand.hpp>
#include <pep/cli/CliApplication.hpp>
#include <pep/cli/Command.hpp>

#include <memory>
#include <vector>

namespace pep::cli {

class CommandPseudonym : public ChildCommandOf<CliApplication> {
public:
  explicit CommandPseudonym(CliApplication& parent)
    : ChildCommandOf<CliApplication>("pseudonym", "operations on pseudonyms", parent) {}

private:
  class CommandConvert;

protected:
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;
};

}
