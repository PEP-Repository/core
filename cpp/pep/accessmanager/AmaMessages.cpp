#include <pep/accessmanager/AmaMessages.hpp>
#include <pep/utils/CollectionUtils.hpp>

namespace pep {

size_t AmaQRColumnGroup::FillToProtobufSerializationCapacity(AmaQRColumnGroup& dest, size_t cap, const AmaQRColumnGroup& source, size_t offset, size_t padding) {
  assert(offset == 0 || offset < source.columns.size());
  assert(!source.name.empty());
  size_t paddedNameLength = source.name.length() + padding;
  if (paddedNameLength > cap) {
    return 0; // sentinel value indicating not even the name fits in the new group.
  }
  dest.name = source.name;
  return paddedNameLength + FillToCapacity(std::back_inserter(dest.columns), cap - paddedNameLength, std::ranges::drop_view{ source.columns, static_cast<ptrdiff_t>(offset) }, padding);
}

bool AmaMutationRequest::hasDataAdminOperation() const {
  // if any of these operations are present, the Data Admin accessgroup is required:
  return (!this->createColumn.empty() ||
    !this->removeColumn.empty() ||
    !this->createColumnGroup.empty() ||
    !this->removeColumnGroup.empty() ||
    !this->addColumnToGroup.empty() ||
    !this->removeColumnFromGroup.empty() ||
    !this->createParticipantGroup.empty() ||
    !this->removeParticipantGroup.empty() ||
    !this->addParticipantToGroup.empty() ||
    !this->removeParticipantFromGroup.empty());
}
bool AmaMutationRequest::hasAccessAdminOperation() const {
  // if any of these operations are present, the Access Admin accessgroup is required:
  return (!this->createColumnGroupAccessRule.empty() ||
    !this->removeColumnGroupAccessRule.empty() ||
    !this->createParticipantGroupAccessRule.empty() ||
    !this->removeParticipantGroupAccessRule.empty());
}

}
