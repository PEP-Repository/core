#include <pep/metrics/RegisteredMetrics.hpp>

#include <prometheus/registry.h>

#include <memory>
#include <stdexcept>

namespace pep {

RegisteredMetrics::RegisteredMetrics(std::shared_ptr<prometheus::Registry> registry)
  : registry_(registry) {
  if (registry_ == nullptr) {
    throw std::runtime_error("Metrics registration requires a non-null Registry");
  }
}

}
