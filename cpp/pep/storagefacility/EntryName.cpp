#include <pep/storagefacility/EntryName.hpp>

namespace pep {

EntryName::EntryName(std::string participant, bool validateParticipant, std::string column)
  : mParticipant(std::move(participant)), mColumn(std::move(column)) {
  if (validateParticipant) {
    try {
      (void)this->pseudonym(); // Raises an exception if mParticipant cannot be parsed
    }
    catch (const std::exception& error) {
      throw std::runtime_error(std::string("Invalid entry participant name: ") + error.what());
    }
  }

  if (mColumn.empty()) {
    throw std::runtime_error("Invalid entry column name: may not be empty");
  }
  auto pos = mColumn.find(DELIMITER);
  if (pos != std::string::npos) {
    throw std::runtime_error(std::string("Invalid entry column name: may not contain entry name delimiter ") + DELIMITER);
  }
}

EntryName EntryName::Parse(const std::string& sfId) {
  auto pos = sfId.find(DELIMITER);
  if (pos == std::string::npos) {
    throw std::runtime_error("Invalid file store entry name: does not contain a delimiter");
  }
  return EntryName(sfId.substr(0, pos), sfId.substr(pos + 1));
}

}
