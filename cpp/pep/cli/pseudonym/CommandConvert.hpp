#pragma once

#include <pep/cli/Pseudonym.hpp>
#include <rxcpp/operators/rx-map.hpp>

namespace pep::cli {

class CommandPseudonym::CommandConvert: public ChildCommandOf<CommandPseudonym> {
public:
  explicit CommandConvert(CommandPseudonym &parent)
  : ChildCommandOf<CommandPseudonym>("convert", "Convert a pseudonym from one type into another", parent)
  {}

protected:
  int execute() override;
  commandline::Parameters getSupportedParameters() const override;
  std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#psuedonym-convert"; }
};

} // namespace pep::cli
