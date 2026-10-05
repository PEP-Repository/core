#include <pep/metrics/RegisteredMetrics.hpp>

namespace pep {

RegisteredMetrics::RegisteredMetrics(std::shared_ptr<prometheus::Registry> registry)
  : registry_(registry) {
  if (registry_ == nullptr) {
    throw std::runtime_error("Metrics registration requires a non-null Registry");
  }
}

}
