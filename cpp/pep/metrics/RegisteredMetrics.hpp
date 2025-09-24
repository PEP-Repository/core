#pragma once

#include <prometheus/registry.h>

namespace pep {

/*!
  * \brief Base class for (Prometheus) metrics. Ensures the Registry stays alive for as long as the metrics (instance) exists.
  */
class RegisteredMetrics {
private:
  std::shared_ptr<prometheus::Registry> mRegistry;

protected:
  explicit RegisteredMetrics(std::shared_ptr<prometheus::Registry> registry);

  inline std::shared_ptr<prometheus::Registry> getRegistry() noexcept { return mRegistry; }
};

}
