#include <pep/application/CommandLineUtility.hpp>
#include <pep/utils/Log.hpp> // for Severity

#include <optional>

namespace pep {
namespace commandline {

std::optional<Severity> Utility::syslogLogMinimumSeverityLevel() const {
  return std::nullopt;
}

}
}