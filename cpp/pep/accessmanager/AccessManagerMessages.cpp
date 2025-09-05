#include <pep/accessmanager/AccessManagerMessages.hpp>

#include <format>

namespace pep {

std::shared_ptr<SignedTicket2> IndexedTicket2::getTicket() const {
  return mTicket;
}

std::vector<std::string> IndexedTicket2::getColumnGroups() const {
  std::vector<std::string> ret;
  ret.reserve(mColumnGroups.size());
  for (const auto& kv : mColumnGroups)
    ret.push_back(kv.first);
  return ret;
}

std::vector<std::string> IndexedTicket2::getParticipantGroups() const {
  std::vector<std::string> ret;
  ret.reserve(mParticipantGroups.size());
  for (const auto& kv : mParticipantGroups)
    ret.push_back(kv.first);
  return ret;
}

std::vector<std::string> IndexedTicket2::getColumns() const {
  std::vector<std::string> ret;
  const auto& columns = openTicketWithoutCheckingSignature()->mColumns;
  ret.reserve(columns.size());
  for (const auto& column : columns)
    ret.push_back(column);
  return ret;
}

std::vector<std::string> IndexedTicket2::getModes() const {
  std::vector<std::string> ret;
  const auto& modes = openTicketWithoutCheckingSignature()->mModes;
  ret.reserve(modes.size());
  for (const auto& mode : modes)
    ret.push_back(mode);
  return ret;
}


std::vector<PolymorphicPseudonym> IndexedTicket2::getPolymorphicPseudonyms() const {
  return openTicketWithoutCheckingSignature()->getPolymorphicPseudonyms();
}

std::shared_ptr<Ticket2> IndexedTicket2::openTicketWithoutCheckingSignature() const {
  std::lock_guard<std::mutex> lock(mUnpackedTicketLock);
  if (mUnpackedTicket == nullptr)
    mUnpackedTicket = std::make_shared<Ticket2>(
      mTicket->openWithoutCheckingSignature());
  return mUnpackedTicket;
}

const std::unordered_map<std::string, IndexList>&
IndexedTicket2::getColumnGroupMapping() const {
  return mColumnGroups;
}

const std::unordered_map<std::string, IndexList>&
IndexedTicket2::getParticipantGroupMapping() const {
  return mParticipantGroups;
}

std::string StructureMetadataKey::toString() const {
  return std::format("{}:{}", metadataGroup, subkey);
}

}
