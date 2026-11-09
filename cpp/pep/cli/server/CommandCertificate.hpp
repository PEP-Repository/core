#pragma once

#include <pep/application/CommandLineCommand.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/Server.hpp>

#include <memory>
#include <vector>

class pep::cli::CommandServer::CommandCertificate : public ChildCommandOf<CommandServer> {
public:
  explicit CommandCertificate(CommandServer&);

private:
  class CommandRequestCSR;
  class CommandReplace;
  class CommandCommit;

protected:
  std::vector<std::shared_ptr<Command>> createChildCommands() override;
};