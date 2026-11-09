#pragma once

#include <pep/application/CommandLineCommand.hpp>
#include <pep/application/CommandLineParameter.hpp> // for commandline::Parameters
#include <pep/application/CommandLineUtility.hpp>
#include <pep/utils/Configuration_fwd.hpp>

#include <memory>
#include <string>
#include <vector>

namespace pep {

class EnrollmentApplication : public commandline::Utility {
protected:
  std::string getDescription() const override;
  commandline::Parameters getSupportedParameters() const override;
  std::vector<std::shared_ptr<commandline::Command>> createChildCommands() override;

public:
  Configuration getConfiguration();
};

}
