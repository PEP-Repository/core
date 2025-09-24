#pragma once

#include <pep/castor/DataPoint.hpp>
#include <pep/castor/Field.hpp>

namespace pep {
namespace castor {

/*!
  * \brief Combination of a DataPointBase and the Field that produced the data.
  */
class FieldValue {
private:
  std::shared_ptr<Field> mField;
  std::shared_ptr<DataPointBase> mDataPoint;

  void addTo(boost::property_tree::ptree& destination) const;

public:
  FieldValue(std::shared_ptr<Field> field, std::shared_ptr<DataPointBase> dataPoint);

  inline std::shared_ptr<const Field> getField() const noexcept { return mField; }

  /*!
  * \brief Writes the specified FieldValue instances to a Ptree.
  * \param values The values to collect into the Ptree.
  * \return (An observable emitting) a Ptree containing (serialized forms of) the specified field values.
  */
  static rxcpp::observable<std::shared_ptr<boost::property_tree::ptree>> Aggregate(rxcpp::observable<std::shared_ptr<FieldValue>> values);
};

}
}
