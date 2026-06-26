#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class RepeatingDataForm;

class RepeatingData : public SimpleCastorChildObject<RepeatingData, Study>, public SharedConstructor<RepeatingData> {
 private:
  std::string name_;

 public:
  static const std::string RelativeApiEndpoint;
  static const std::string EmbeddedApiNodeName;

  std::string getName() const { return name_; }

  rxcpp::observable<std::shared_ptr<RepeatingDataForm>> getRepeatingDataForms();

 protected:
  /*!
   * \brief construct a RepeatingData
   *
   * \param study The Study this repeating data belongs to
   * \param json The %Json response from the Castor API for this repeating data
   */
  RepeatingData(std::shared_ptr<Study> study, JsonPtr json);

 private:
  friend class SharedConstructor<RepeatingData>;
};

}
}
