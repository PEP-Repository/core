#pragma once

#include <pep/castor/Study.hpp>

namespace pep
{
namespace castor
{

class Visit : public SimpleCastorChildObject<Visit, Study>, public SharedConstructor<Visit> {
 private:
  std::string name_;

 public:
  static const std::string RelativeApiEndpoint;
  static const std::string EmbeddedApiNodeName;

  std::string getName() const { return name_; }

 protected:
  /*!
   * \brief construct a Visit
   *
   * \param study The Study this visit belongs to
   * \param json The %Json response from the Castor API for this visit
   */
   Visit(std::shared_ptr<Study> study, JsonPtr json);

 private:
  friend class SharedConstructor<Visit>;
};

}
}
