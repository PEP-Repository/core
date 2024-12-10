#pragma once

#include <pep/cli/Asa.hpp>
#include <pep/cli/structuredoutput/Common.hpp>

class CommandAsa::CommandAsaQuery : public ChildCommandOf<CommandAsa> {
public:
  explicit CommandAsaQuery(CommandAsa& parent)
    : ChildCommandOf<CommandAsa>("query", "Query state (users, groups, etc.)", parent) {}

protected:
  pep::commandline::Parameters getSupportedParameters() const override;

  int execute() override;

private:
  static structuredOutput::DisplayConfig extractConfig(const pep::commandline::NamedValues& values);

  static pep::AsaQuery extractQuery(const pep::commandline::NamedValues& values);
};
