#include <pep/utils/Configuration.hpp>
#include <pep/utils/Log.hpp>
#include <pep/castor/ApiKey.hpp>

namespace pep {
namespace castor {

namespace {

const std::string LOG_TAG = "Castor API key";

}


ApiKey ApiKey::FromFile(const std::filesystem::path& file) {
  // read the Castor API key from the json file specified in the configuration
  try {
    Configuration apiKeyProperties = Configuration::FromFile(std::filesystem::canonical(file));

    auto id = apiKeyProperties.get<std::string>("ClientKey");
    auto secret = apiKeyProperties.get<std::string>("ClientSecret");

    return ApiKey{ id, secret };
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with Castor API Key file: " << e.what();
    LOG(LOG_TAG, critical) << "Castor API Key file is  " << file << std::endl;
    throw;
  }

}

}
}
