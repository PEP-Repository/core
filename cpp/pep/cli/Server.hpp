#pragma once

#include <pep/application/Application.hpp>
#include <pep/cli/Command.hpp>

namespace pep::cli {

class CommandServer : public ChildCommandOf<CliApplication> {
public:
  explicit CommandServer(CliApplication& parent)
    : ChildCommandOf<CliApplication>("server", "Administer servers", parent) {}

private:
  class CommandCertificate;

protected:
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;
};

}
