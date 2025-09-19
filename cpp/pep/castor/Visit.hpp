#pragma once

#include <pep/castor/Study.hpp>

namespace pep
{
namespace castor
{

class Visit : public SimpleCastorChildObject<Visit, Study>, public SharedConstructor<Visit> {
 private:
  std::string mName;

 public:
  static const std::string RELATIVE_API_ENDPOINT;
  static const std::string EMBEDDED_API_NODE_NAME;

  std::string getName() const { return mName; }

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
