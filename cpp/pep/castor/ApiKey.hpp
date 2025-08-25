#pragma once

#include <filesystem>

namespace pep {
namespace castor {

struct ApiKey {
  const std::string id;
  const std::string secret;

  static ApiKey FromFile(const std::filesystem::path& file);
};

}
}
