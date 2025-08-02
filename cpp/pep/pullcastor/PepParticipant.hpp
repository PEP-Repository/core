#pragma once

#include <pep/core-client/CoreClient_fwd.hpp>
#include <pep/pullcastor/CellContent.hpp>
#include <pep/pullcastor/PullCastorUtils.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Loads and provides data belonging to a single participant stored in PEP.
  */
class PepParticipant : public std::enable_shared_from_this<PepParticipant>, private SharedConstructor<PepParticipant> {
  friend class SharedConstructor<PepParticipant>;

private:
  PolymorphicPseudonym mPp;
  UnOrOrderedMap<std::string, std::shared_ptr<CellContent>> mCells;

  explicit PepParticipant(const PolymorphicPseudonym& pp);

  void loadCell(std::shared_ptr<CoreClient> client, std::shared_ptr<SignedTicket2> ticket, const EnumerateAndRetrieveResult& ear);

public:
  /*!
    * \brief Loads the specified columns for all participants from PEP.
    * \param client The client to use for data retrieval.
    * \param participants The participants to load.
    * \param participantGroups The participant groups to load.
    * \param columns The columns to load.
    * \param columnGroups The column groups to load.
    * \return (An observable emitting) PepParticipant instances for all participants stored in PEP.
    */
  static rxcpp::observable<std::shared_ptr<PepParticipant>> LoadAll(
    std::shared_ptr<CoreClient> client,
    const std::vector<PolymorphicPseudonym>& participants,
    const std::vector<std::string>& participantGroups,
    const std::vector<std::string>& columns,
    const std::vector<std::string>& columnGroups);

  /*!
    * \brief Produces the polymorphic pseudonym associated with this participant.
    * \return A PolymorphicPseudonym instance specifying the participant's PP.
    */
  inline const PolymorphicPseudonym& getPp() const noexcept { return mPp; }

  /*!
  * \brief Produces a CellContent instance representing this participant's data for the specified column.
  * \param column The name of the column to retrieve data for.
  * \return A CellContent instance if the participant has data in the specified column, or a nullptr if not.
  */
  std::shared_ptr<const CellContent> tryGetCellContent(const std::string& column) const;
};

}
}
