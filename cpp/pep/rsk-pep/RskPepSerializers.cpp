#include <pep/rsk-pep/RskPepSerializers.hpp>

#include <stdexcept>

namespace pep {

LocalPseudonym Serializer<LocalPseudonym>::fromProtocolBuffer(proto::LocalPseudonym&& source) const {
  try {
    return LocalPseudonym::FromPacked(source.packed());
  } catch (const std::invalid_argument& ex) {
    throw SerializeException(ex.what());
  }
}

void Serializer<LocalPseudonym>::moveIntoProtocolBuffer(proto::LocalPseudonym& dest, LocalPseudonym value) const {
  dest.set_packed(std::string(value.pack()));
}

EncryptedLocalPseudonym Serializer<EncryptedLocalPseudonym>::fromProtocolBuffer(proto::EncryptedLocalPseudonym&& source) const {
  try {
    return EncryptedLocalPseudonym::FromPacked(source.packed());
  } catch (const std::invalid_argument& ex) {
    throw SerializeException(ex.what());
  }
}

void Serializer<EncryptedLocalPseudonym>::moveIntoProtocolBuffer(proto::EncryptedLocalPseudonym& dest, EncryptedLocalPseudonym value) const {
  dest.set_packed(value.pack());
}

PolymorphicPseudonym Serializer<PolymorphicPseudonym>::fromProtocolBuffer(proto::PolymorphicPseudonym&& source) const {
  try {
    return PolymorphicPseudonym::FromPacked(source.packed());
  } catch (const std::invalid_argument& ex) {
    throw SerializeException(ex.what());
  }
}

void Serializer<PolymorphicPseudonym>::moveIntoProtocolBuffer(proto::PolymorphicPseudonym& dest, PolymorphicPseudonym value) const {
  dest.set_packed(value.pack());
}

}
