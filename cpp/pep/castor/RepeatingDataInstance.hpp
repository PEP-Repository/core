#pragma once

#include <pep/castor/Participant.hpp>

namespace pep {
namespace castor {

class RepeatingDataPoint;
class RepeatingData;

/*!
  * \brief A filled in instance of a repeating data (definition).
  * \remark Corresponds to Castor API's "repeating data instance" concept: see https://data.castoredc.com/api#/repeating-data-instance
  */
class RepeatingDataInstance : public SimpleCastorChildObject<RepeatingDataInstance, Participant>, public SharedConstructor<RepeatingDataInstance> {
 private:
  std::string mParticipantId;
  std::string mName;
  bool mArchived;

 public:
  static const std::string RELATIVE_API_ENDPOINT;
  static const std::string EMBEDDED_API_NODE_NAME;

  //! \return The ID of the participant this RepeatingDataInstance belongs to, as returned by the API. This is needed, for example, to filter RepeatingDataInstances that actually belong to a certain participant
  //! \see Participant::getRepeatingDataInstances
  std::string getParticipantId() const{ return mParticipantId; }

  //! \return The participant this RepeatingDataInstance belongs to
  std::shared_ptr<Participant> getParticipant() const { return this->getParent(); }

  std::string getName() const { return mName; }

  //! \return Filled in values for this instance
  rxcpp::observable<std::shared_ptr<RepeatingDataPoint>> getRepeatingDataPoints();

  std::shared_ptr<RepeatingData> getRepeatingData() const { return repeatingData; }

  //! \return Whether the repeating data instance is archived or not
  inline bool isArchived() const { return mArchived; }

  static rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> BulkRetrieve(std::shared_ptr<Study> study, rxcpp::observable<std::shared_ptr<Participant>> participants);

  /*!
   * \brief Helper function to convert API "404 Not Found" results to an empty observable.
   *
   * \param ep An exception thrown during RX pipeline processing.
   * \return An empty observable if the exception indicates that the API returned a "404 Not Found" error. Re-raises the exception otherwise
   *
   * \remark Use as myObs.on_error_resume_next(&RepeatingDataInstance::ConvertNotFoundToEmpty)
   */
  static rxcpp::observable<std::shared_ptr<RepeatingDataInstance>> ConvertNotFoundToEmpty(std::exception_ptr ep);

 protected:
  /*!
   * \brief Construct a new RepeatingDataInstance
   *
   * \param participant The Participant this RepeatingDataInstance belongs to
   * \param json The %Json response from the Castor API for this RepeatingDataInstance
   */
  RepeatingDataInstance(std::shared_ptr<Participant> participant, JsonPtr json);

 private:
  std::shared_ptr<RepeatingData> repeatingData;

  friend class SharedConstructor<RepeatingDataInstance>;
};

}
}
