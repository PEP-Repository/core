#include <pep/accessmanager/AmaMessages.hpp>
#include <pep/utils/CollectionUtils.hpp>

namespace pep {

size_t AmaQRColumnGroup::FillToProtobufSerializationCapacity(AmaQRColumnGroup& dest, const AmaQRColumnGroup& source, const size_t& cap, const size_t& offset, const size_t& padding) {
  assert(offset == 0 || offset < source.columns_.size());
  assert(!source.name_.empty());
  size_t paddedNameLength = source.name_.length() + padding;
  if (paddedNameLength > cap) {
    return 0; // sentinel value indicating not even the name fits in the new group.
  }
  dest.name_ = source.name_;
  return paddedNameLength + FillVectorToCapacity(dest.columns_, source.columns_, cap - paddedNameLength, offset, padding);
}

bool AmaMutationRequest::hasDataAdminOperation() const {
  // if any of these operations are present, the Data Admin accessgroup is required:
  return (!this->createColumn_.empty() ||
    !this->removeColumn_.empty() ||
    !this->createColumnGroup_.empty() ||
    !this->removeColumnGroup_.empty() ||
    !this->addColumnToGroup_.empty() ||
    !this->removeColumnFromGroup_.empty() ||
    !this->createParticipantGroup_.empty() ||
    !this->removeParticipantGroup_.empty() ||
    !this->addParticipantToGroup_.empty() ||
    !this->removeParticipantFromGroup_.empty());
}
bool AmaMutationRequest::hasAccessAdminOperation() const {
  // if any of these operations are present, the Access Admin accessgroup is required:
  return (!this->createColumnGroupAccessRule_.empty() ||
    !this->removeColumnGroupAccessRule_.empty() ||
    !this->createParticipantGroupAccessRule_.empty() ||
    !this->removeParticipantGroupAccessRule_.empty());
}

}
