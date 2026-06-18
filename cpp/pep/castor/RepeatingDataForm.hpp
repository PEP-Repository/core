#pragma once

#include <pep/castor/RepeatingData.hpp>

namespace pep {
namespace castor {

class RepeatingDataForm : public SimpleCastorChildObject<RepeatingDataForm, RepeatingData>, public SharedConstructor<RepeatingDataForm> {
 private:
  std::string name_;
  int number_;

 public:
  static const std::string RelativeApiEndpoint;
  static const std::string EmbeddedApiNodeName;

  std::string getName() const { return name_; }
  int getNumber() const { return number_; }

  std::shared_ptr<RepeatingData> getRepeatingData() const { return this->getParent(); }

 protected:
  /*!
   * \brief construct a RepeatingDataForm
   *
   * \param repeatingData The Study this repeating data form belongs to
   * \param json The %Json response from the Castor API for this repeating data form
   */
  RepeatingDataForm(std::shared_ptr<RepeatingData> repeatingData, JsonPtr json);

 private:
  friend class SharedConstructor<RepeatingDataForm>;
};

}
}
