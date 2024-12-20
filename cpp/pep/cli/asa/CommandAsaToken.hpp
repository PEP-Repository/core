#pragma once

#include <pep/cli/Asa.hpp>

class CommandAsa::CommandAsaToken : public ChildCommandOf<CommandAsa> {
public:
  explicit CommandAsaToken(CommandAsa& parent)
    : ChildCommandOf<CommandAsa>("token", "Administer access tokens", parent) {}

private:
  class RequestTokenCommand;
  class CommandAsaTokenBlock;

  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;
};
