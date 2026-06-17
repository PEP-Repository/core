#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class Visit;

class Form : public SimpleCastorChildObject<Form, Study>, public SharedConstructor<Form> {
  friend class SharedConstructor<Form>;
 private:
  std::string name_;
  int order_;
  std::shared_ptr<Visit> visit_;

 public:
  static const std::string RELATIVE_API_ENDPOINT;
  static const std::string EMBEDDED_API_NODE_NAME;

  std::string getName() const { return name_; }

  int getOrder() const { return order_; }

  std::shared_ptr<Visit> getVisit() const { return visit_; }

 protected:
  /*!
   * \brief construct a Form
   *
   * \param study The Study this form belongs to
   * \param json The %Json response from the Castor API for this form
   */
   Form(std::shared_ptr<Study> study, JsonPtr json);
};

}
}
