#pragma once

#include <pep/cli/Server.hpp>

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