#include <pep/accessmanager/AmaMessages.hpp>
#include <pep/utils/CollectionUtils.hpp>

namespace pep {

size_t AmaQRColumnGroup::FillToProtobufSerializationCapacity(AmaQRColumnGroup& dest, const AmaQRColumnGroup& source, const size_t& cap, const size_t& offset, const size_t& padding) {
  assert(offset == 0 || offset < source.mColumns.size());
  assert(!source.mName.empty());
  size_t paddedNameLength = source.mName.length() + padding;
  if (paddedNameLength > cap) {
    return 0; // sentinel value indicating not even the name fits in the new group.
  }
  dest.mName = source.mName;
  return paddedNameLength + FillVectorToCapacity(dest.mColumns, source.mColumns, cap - paddedNameLength, offset, padding);
}

bool AmaMutationRequest::hasDataAdminOperation() const {
  // if any of these operations are present, the Data Admin accessgroup is required:
  return (!this->mCreateColumn.empty() ||
    !this->mRemoveColumn.empty() ||
    !this->mCreateColumnGroup.empty() ||
    !this->mRemoveColumnGroup.empty() ||
    !this->mAddColumnToGroup.empty() ||
    !this->mRemoveColumnFromGroup.empty() ||
    !this->mCreateParticipantGroup.empty() ||
    !this->mRemoveParticipantGroup.empty() ||
    !this->mAddParticipantToGroup.empty() ||
    !this->mRemoveParticipantFromGroup.empty());
}
bool AmaMutationRequest::hasAccessAdminOperation() const {
  // if any of these operations are present, the Access Admin accessgroup is required:
  return (!this->mCreateColumnGroupAccessRule.empty() ||
    !this->mRemoveColumnGroupAccessRule.empty() ||
    !this->mCreateParticipantGroupAccessRule.empty() ||
    !this->mRemoveParticipantGroupAccessRule.empty());
}

}
