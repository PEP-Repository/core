#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class Visit;

class Form : public SimpleCastorChildObject<Form, Study>, public SharedConstructor<Form> {
 private:
  std::string mName;
  int mOrder;

 public:
  static const std::string RELATIVE_API_ENDPOINT;
  static const std::string EMBEDDED_API_NODE_NAME;

  std::string getName() const { return mName; }

  int getOrder() const { return mOrder; }

  std::shared_ptr<Visit> getVisit() const { return visit; }

 protected:
  /*!
   * \brief construct a Form
   *
   * \param study The Study this form belongs to
   * \param json The %Json response from the Castor API for this form
   */
   Form(std::shared_ptr<Study> study, JsonPtr json);

 private:
  std::shared_ptr<Visit> visit;

  friend class SharedConstructor<Form>;
};

}
}
