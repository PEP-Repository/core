#pragma once

#include <pep/application/CommandLineUtility.hpp>

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
