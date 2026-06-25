#pragma once

#include <pep/application/Application.hpp>
#include <pep/cli/Command.hpp>

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
