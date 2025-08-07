#include <pep/utils/RegisteredTestEnvironment.hpp>

namespace pep {

std::optional<RegisteredTestEnvironment::Factory>& RegisteredTestEnvironment::RegisteredFactory() {
  static std::optional<Factory> result;
  return result;
}


void RegisteredTestEnvironment::RegisterFactory(const Factory& factory) {
  auto& registered = RegisteredFactory();
  if (registered.has_value()) {
    throw std::runtime_error("Only a single test environment (type) may be registered");
  }
  registered = factory;
}

RegisteredTestEnvironment* RegisteredTestEnvironment::Create(int argc, char* argv[]) {
  const auto& registered = RegisteredFactory();
  if (registered.has_value()) {
    return (*registered)(argc, argv);
  }
  return nullptr;
}

}
