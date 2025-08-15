#pragma once

#include <pep/cli/User.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/accessmanager/UserMessages.hpp>

class pep::cli::CommandUser::CommandUserQuery : public ChildCommandOf<CommandUser> {
public:
  explicit CommandUserQuery(CommandUser& parent)
    : ChildCommandOf<CommandUser>("query", "Query state (users, groups, etc.)", parent) {}

protected:
  pep::commandline::Parameters getSupportedParameters() const override;

  int execute() override;

private:
  static pep::structuredOutput::DisplayConfig extractConfig(const pep::commandline::NamedValues& values);

  static pep::UserQuery extractQuery(const pep::commandline::NamedValues& values);
};
