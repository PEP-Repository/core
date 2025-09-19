#include <pep/rsk/RskKeys.hpp>

#include <pep/crypto/ConstTime.hpp>
#include <pep/utils/CollectionUtils.hpp>

#include <stdexcept>

using namespace pep;

namespace {

void CheckNonzero(std::span<const std::byte> data) {
  if (const_time::IsZero(data)) {
    throw std::invalid_argument("Key cannot be zero");
  }
}

} // namespace

KeyFactorSecret::KeyFactorSecret(std::span<const std::byte, 64> key)
    : hmacKey_(SpanToArray(key)) {
  CheckNonzero(hmacKey_);
}

MasterPrivateKeyShare::MasterPrivateKeyShare(std::span<const std::byte, 32> key)
    : curveScalar_(SpanToArray(key)) {
  CheckNonzero(curveScalar_);
}
