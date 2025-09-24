#pragma once

#include <pep/application/Application.hpp>

namespace pep {
namespace commandline {

class Utility : public pep::Application {
protected:
  std::optional<severity_level> syslogLogMinimumSeverityLevel() const override;
};

}
}
