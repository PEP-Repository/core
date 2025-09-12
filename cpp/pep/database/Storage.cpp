#include <pep/database/Storage.hpp>

namespace pep::database {

const char* const BasicStorage::STORE_IN_MEMORY = ":memory:";

BasicStorage::BasicStorage(const std::string& path)
  : isPersistent(path != STORE_IN_MEMORY) {
}

}
