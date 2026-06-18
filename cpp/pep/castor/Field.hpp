#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class OptionGroup;

class Field : public ParentedCastorObject<Study>, public SharedConstructor<Field> {
 private:
  std::string parentId_;
  int number_;
  std::string type_;
  std::string variableName_;
  std::string label_;
  bool required_;
  std::string units_;
  std::string info_;
  bool hidden_;
  std::string reportId_;
  std::shared_ptr<OptionGroup> optionGroup_;

 public:
  static const std::string TYPE_CHECKBOX;
  static const std::string TYPE_REPEATED_MEASURE;

  //! \return A url that can be used to retrieve this Field object from the Castor API
  std::string makeUrl() const override;

  std::shared_ptr<OptionGroup> getOptionGroup() const { return optionGroup_; }

  std::string getParentId() const { return parentId_; }

  int getNumber() const { return number_; }
  std::string getType() const { return type_; }
  std::string getVariableName() const { return variableName_; }
  std::string getLabel() const { return label_; }
  bool isRequired() const { return required_; }
  std::string getUnits() const { return units_; }
  std::string getInfo() const { return info_; }
  bool isHidden() const { return hidden_; }

  /* See e-mail from support@pep.cs.ru.nl to Castor support dated 23/02/2023, 10:13.
   * For Field instances with TYPE_REPEATED_MEASURE, we use the "report_id" property to determine
   * which RepeatingData is associated with the field. Unfortunately the property is no longer
   * included in the Castor API documentation at https://data.castoredc.com/api , and its name
   * is associated with an older API version that has been deprecated. We can only hope that
   * the Castor API will keep producing the property (or an equivalent one).
   */
  std::string getReportId() const { return reportId_; }

  static rxcpp::observable<std::shared_ptr<Field>> RetrieveForParent(std::shared_ptr<Study> study);

 protected:
  Field(std::shared_ptr<Study> study, JsonPtr json);

 private:
  friend class SharedConstructor<Field>;
};

}
}
