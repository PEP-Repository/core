#include <pep/cli/Server.hpp>
#include <pep/cli/server/CommandCertificate.hpp>
#include <pep/cli/Commands.hpp>


namespace pep::cli {

std::shared_ptr<ChildCommandOf<CliApplication>> CreateCommandServer(CliApplication& parent) {
  return std::make_shared<CommandServer>(parent);
}

std::vector<std::shared_ptr<pep::commandline::Command>> CommandServer::createChildCommands() {
  return {
    std::make_shared<CommandCertificate>(*this),
  };
}

}
