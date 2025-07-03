#pragma once

#include <pep/cli/Command.hpp>

namespace pep::cli {

class CommandToken : public ChildCommandOf<CliApplication> {
public:
  explicit CommandToken(CliApplication& parent)
    : ChildCommandOf<CliApplication>("token", "Administer access tokens", parent) {}

private:
  class RequestTokenCommand;
  class CommandTokenBlock;

  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;
};

}
