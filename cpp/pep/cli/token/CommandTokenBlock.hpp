#pragma once

#include <pep/cli/Token.hpp>

#include <rxcpp/operators/rx-map.hpp>

/// CLI command to manage which authentication tokens are blocked.
class pep::cli::CommandToken::CommandTokenBlock : public ChildCommandOf<CommandToken> {
public:
  explicit CommandTokenBlock(CommandToken& parent)
    : ChildCommandOf<CommandToken>("block", "Manage blocked authentication tokens.", parent) {}

protected:
  std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#token-block"; }

  std::vector<std::shared_ptr<Command>> createChildCommands() override;

private:
  class SubcommandCreate; ///< Subcommand for the creation of new blocking rules.
  class SubcommandRemove; ///< Subcommand for the removal of existing blocking rules.
  class SubcommandList;   ///< Subcommand to list all existing blocking rules.
};

