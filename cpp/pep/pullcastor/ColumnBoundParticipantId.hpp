#pragma once

#include <functional>
#include <string>

namespace pep {
namespace castor {

/*!
  * \brief Associates a Castor participant ID with a PEP (short pseudonym) column name.
  * \remark Introduced to eliminate trouble due to duplicate participant IDs in different Castor studies: see #1603.
  */
class ColumnBoundParticipantId {
private:
  std::string mColumnName;
  std::string mParticipantId;

public:
  /*!
    * \brief Constructor.
    * \param columnName The PEP (SP) column name associated with the specified Castor participant ID.
    * \param participantId The Castor participant ID.
    */
  ColumnBoundParticipantId(const std::string& columnName, const std::string& participantId);

  /*!
    * \brief Property accessor for the column name.
    * \return The PEP (SP) column name associated with this instance's Castor participant ID.
    */
  inline const std::string& getColumnName() const { return mColumnName; }

  /*!
    * \brief Property accessor for the Castor participant ID.
    * \return The Castor participant ID.
    */
  inline const std::string& getParticipantId() const { return mParticipantId; }
};

/*!
  * \brief Equality comparison operator for ColumnBoundParticipantId instances.
  * \param lhs the first ColumnBoundParticipantId instance to compare.
  * \param rhs the second ColumnBoundParticipantId instance to compare.
  * \return TRUE if the instances are equal; FALSE if not.
  */
bool operator==(const ColumnBoundParticipantId& lhs, const ColumnBoundParticipantId& rhs);

}
}

namespace std {

/*!
  * \brief (Non-cryptographic) hash function for ColumnBoundParticipantId instances.
  * \remark A.o. allows ColumnBoundParticipantId to be used as the key type in std::unordered_map<>.
  */
template <>
struct hash<pep::castor::ColumnBoundParticipantId> {
  /*!
    * \brief Produces a hash value for the specified ColumnBoundParticipantId instance.
    * \param cbpId the instance to hash.
    * \return the hash value for the ColumnBoundParticipantId instance.
    */
  size_t operator()(const pep::castor::ColumnBoundParticipantId& cbpId) const;
};

}
