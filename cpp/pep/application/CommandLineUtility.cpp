#include <pep/application/CommandLineUtility.hpp>

namespace pep {
namespace commandline {

std::optional<Severity> Utility::syslogLogMinimumSeverityLevel() const {
  return std::nullopt;
}

}
}