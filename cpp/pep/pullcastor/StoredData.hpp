#pragma once

#include <pep/core-client/CoreClient_fwd.hpp>
#include <pep/pullcastor/ColumnBoundParticipantId.hpp>
#include <pep/pullcastor/PepParticipant.hpp>
#include <pep/pullcastor/StorableContent.hpp>
#include <pep/pullcastor/StudyAspect.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Provides participant data stored in PEP.
  */
class StoredData : public std::enable_shared_from_this<StoredData>, public SharedConstructor<StoredData> {
  friend class SharedConstructor<StoredData>;

private:
  using ParticipantsByColumnBoundParticipantId = std::unordered_map<ColumnBoundParticipantId, std::shared_ptr<PepParticipant>>;
  std::shared_ptr<ParticipantsByColumnBoundParticipantId> mParticipants;

  explicit StoredData(std::shared_ptr<ParticipantsByColumnBoundParticipantId> participants);

  std::shared_ptr<PepParticipant> tryGetParticipant(const ColumnBoundParticipantId& cbrId) const noexcept;

public:
  /*!
    * \brief Creates a StoredData instance, which is populated data from PEP('s Storage Facility).
    * \param participants Participant PPs to retrieve.
    * \param spColumns Short pseudonym columns to retrieve.
    * \param otherColumns Other columns to retrieve.
    * \return (An observable emitting a single) StoredData instance containing the current data in PEP.
    * \remark Retrieves data for all participants (the "*" group) if the "participants" vector is empty.
    */
  static rxcpp::observable<std::shared_ptr<StoredData>> Load(
    std::shared_ptr<CoreClient> client,
    std::shared_ptr<std::vector<PolymorphicPseudonym>> participants,
    std::shared_ptr<std::vector<std::string>> spColumns,
    std::shared_ptr<std::vector<std::string>> otherColumns);

  /*!
    * \brief Determines if the specified Castor participant ID corresponds with a short pseudonym known to (stored in) PEP.
    * \param cbpId The column-bound Castor participant ID to look up.
    * \return TRUE if some participant is associated with that short pseudonym; FALSE if not.
    */
  bool hasCastorParticipantId(const ColumnBoundParticipantId& cbpId) const noexcept;

  /*!
    * \brief Returns a StoreData2Entry if the specified StorableCellContent is not yet in PEP, or if a different value is currently stored in the associated cell.
    * \param storable The data that should be stored in PEP.
    * \return (An observable emitting a single) StoreData2Entry if PEP must be updated, or an empty observable if equal data is already present in PEP.
    */
  rxcpp::observable<StoreData2Entry> getUpdateEntry(std::shared_ptr<StorableCellContent> storable);

  /*!
    * \brief Produces PepParticipant entries for all participants that this instance knows about.
    * \return (An observable emitting) PepParticipant objects for every participant for which data was received from PEP('s Storage Facility).
    */
  rxcpp::observable<std::shared_ptr<const PepParticipant>> getParticipants() const;

  /*!
    * \brief Produces the short pseudonyms associated with the specified participant (record).
    * \param participant The participant record whose SPs should be produced
    * \param spColumnName The name of the PEP column containing short pseudonym values
    * \return (An observable emitting) short pseudonym values for the specified PepParticipant.
    * \remark The std::shared_ptr<const PepParticipant> must be one that was received from the .getParticipants() method.
    */
  rxcpp::observable<std::string> getCastorSps(std::shared_ptr<const PepParticipant> participant, const std::string& spColumnName) const;
};

}
}
