#pragma once

#include <pep/application/Application.hpp>
#include <pep/utils/Log.hpp> // for Severity

#include <optional>

namespace pep {
namespace commandline {

class Utility : public pep::Application {
protected:
  std::optional<Severity> syslogLogMinimumSeverityLevel() const override;
};

}
}
