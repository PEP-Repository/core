#pragma once

#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/cli/Command.hpp>
#include <pep/cli/Pseudonym.hpp>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-map.hpp>

#include <optional>

namespace pep::cli {

class CommandPseudonym::CommandConvert: public ChildCommandOf<CommandPseudonym> {
public:
  explicit CommandConvert(CommandPseudonym &parent)
  : ChildCommandOf<CommandPseudonym>("convert", "Convert a pseudonym from one type into another", parent)
  {}

protected:
  int execute() override;
  commandline::Parameters getSupportedParameters() const override;
  std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#pseudonym-convert"; }
};

} // namespace pep::cli
