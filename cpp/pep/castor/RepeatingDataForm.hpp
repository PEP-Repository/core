#pragma once

#include <pep/castor/RepeatingData.hpp>

namespace pep {
namespace castor {

class RepeatingDataForm : public SimpleCastorChildObject<RepeatingDataForm, RepeatingData>, public SharedConstructor<RepeatingDataForm> {
 private:
  std::string mName;
  int mNumber;

 public:
  static const std::string RELATIVE_API_ENDPOINT;
  static const std::string EMBEDDED_API_NODE_NAME;

  std::string getName() const { return mName; }
  int getNumber() const { return mNumber; }

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
