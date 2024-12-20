#pragma once

#include <pep/cli/asa/CommandAsaToken.hpp>

#include <rxcpp/operators/rx-map.hpp>

/// CLI command to manage which authentication tokens are blocked.
class CommandAsa::CommandAsaToken::CommandAsaTokenBlock : public ChildCommandOf<CommandAsa::CommandAsaToken> {
public:
  explicit CommandAsaTokenBlock(CommandAsa::CommandAsaToken& parent)
    : ChildCommandOf<CommandAsa::CommandAsaToken>("block", "Manage blocked authentication tokens.", parent) {}

protected:
  std::optional<std::string> getRelativeDocumentationUrl() const override { return "Using-pepcli#token-block"; }

  std::vector<std::shared_ptr<Command>> createChildCommands() override;

private:
  class SubcommandCreate; ///< Subcommand for the creation of new blocking rules.
  class SubcommandRemove; ///< Subcommand for the removal of existing blocking rules.
  class SubcommandList;   ///< Subcommand to list all existing blocking rules.
};
