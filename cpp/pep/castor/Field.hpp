#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class OptionGroup;

class Field : public ParentedCastorObject<Study>, public SharedConstructor<Field> {
 private:
  std::string mParentId;
  int mNumber;
  std::string mType;
  std::string mVariableName;
  std::string mLabel;
  bool mRequired;
  std::string mUnits;
  std::string mInfo;
  bool mHidden;
  std::string mReportId;
  std::shared_ptr<OptionGroup> mOptionGroup;

 public:
  static const std::string TYPE_CHECKBOX;
  static const std::string TYPE_REPEATED_MEASURE;

  //! \return A url that can be used to retrieve this Field object from the Castor API
  std::string makeUrl() const override;

  std::shared_ptr<OptionGroup> getOptionGroup() const { return mOptionGroup; }

  std::string getParentId() const { return mParentId; }

  int getNumber() const { return mNumber; }
  std::string getType() const { return mType; }
  std::string getVariableName() const { return mVariableName; }
  std::string getLabel() const { return mLabel; }
  bool isRequired() const { return mRequired; }
  std::string getUnits() const { return mUnits; }
  std::string getInfo() const { return mInfo; }
  bool isHidden() const { return mHidden; }

  /* See e-mail from support@pep.cs.ru.nl to Castor support dated 23/02/2023, 10:13.
   * For Field instances with TYPE_REPEATED_MEASURE, we use the "report_id" property to determine
   * which RepeatingData is associated with the field. Unfortunately the property is no longer
   * included in the Castor API documentation at https://data.castoredc.com/api , and its name
   * is associated with an older API version that has been deprecated. We can only hope that
   * the Castor API will keep producing the property (or an equivalent one).
   */
  std::string getReportId() const { return mReportId; }

  static rxcpp::observable<std::shared_ptr<Field>> RetrieveForParent(std::shared_ptr<Study> study);

 protected:
  Field(std::shared_ptr<Study> study, JsonPtr json);

 private:
  friend class SharedConstructor<Field>;
};

}
}
