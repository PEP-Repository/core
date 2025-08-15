#pragma once

#include <pep/castor/Participant.hpp>

namespace pep {
namespace castor {

enum DataPointType { STUDY, SURVEY, REPEATING };

class DataPointBase : public CastorObject {
 private:
  std::string mValue;

 public:
  std::string getValue() const {
    return mValue;
  }

  virtual std::shared_ptr<Participant> getParticipant() const = 0;

  virtual DataPointType getType() const = 0;

 protected:
  static const std::string EMBEDDED_API_NODE_NAME;

  /*!
   * \brief Construct a new DataPointBase
   *
   * \param json The %Json response from the Castor API for this DataPointBase
   */
  DataPointBase(JsonPtr json);

  static std::string GetApiRoot(std::shared_ptr<Study> study, const std::string& relative);
  static std::string GetApiRoot(std::shared_ptr<Participant> participant, const std::string& relative);

  template <typename TParent>
  static std::string GetApiRoot(std::shared_ptr<TParent> parent, const std::string& relative) {
    return GetApiRoot(parent->getParticipant(), relative) + "/" + parent->getId();
  }
};

/*!
 * \brief Utility base for data point types.
 *
 * \remark Inheritors must define static const strings RELATIVE_API_ENDPOINT for their data point type.
 */
template<class ChildType, class ParentType>
class DataPoint : public ParentedCastorObject<ParentType, DataPointBase> {
public:
  /*!
   * \brief Retrieves all data point instances belonging to the specified parent.
   *
   * \param parent The parent to retrieve data points for.
   */
  static rxcpp::observable<std::shared_ptr<ChildType>> RetrieveForParent(std::shared_ptr<ParentType> parent) {
    return CastorObject::RetrieveList<ChildType, ParentType>(
      parent,
      DataPointBase::GetApiRoot(parent, ChildType::RELATIVE_API_ENDPOINT),
      DataPointBase::EMBEDDED_API_NODE_NAME);
  }

private:
  friend ChildType;
  /*!
   * \brief Construct a new DataPoint
   *
   * \param parent The parent this data point belongs to
   * \param json The %Json response from the Castor API for this DataPoint
   */
  DataPoint(std::shared_ptr<ParentType> parent, JsonPtr json)
    : ParentedCastorObject<ParentType, DataPointBase>(parent, json) {}
};

}
}
