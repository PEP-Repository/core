#include <pep/application/CommandLineUtility.hpp>

namespace pep {
namespace commandline {

std::optional<severity_level> Utility::syslogLogMinimumSeverityLevel() const {
  return std::nullopt;
}

}
}