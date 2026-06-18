#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

/*!
 * \brief Class representing a Site in the Castor API
 *
 * Sites are created in each study. The same site in different studies is therefore represented by different instances of this class
 */
class Site : public SimpleCastorChildObject<Site, Study>, public SharedConstructor<Site> {
 private:
  std::string abbreviation_;
  std::string name_;

 public:
  static const std::string RelativeApiEndpoint;
  static const std::string EmbeddedApiNodeName;

  //! \return The abbreviation for the site, which can be uses as an identifier for this site (like the study slug)
  std::string getAbbreviation() const;
  //! \return The full name of the site
  std::string getName() const;

 protected:
  /*!
   * \brief construct a Site
   *
   * \param study The Study this site belongs to
   * \param json The %Json response from the Castor API for this site
   */
   Site(std::shared_ptr<Study> study, JsonPtr json);

 private:
  friend class SharedConstructor<Site>;
};

}
}
