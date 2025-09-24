#include <pep/registrationserver/RegistrationServer.hpp>
#include <pep/service-application/ServiceApplication.hpp>

/*! \brief entry point for registration server
*
* \param argc number of arguments
* \param argv array of strings containing arguments
*/
PEP_DEFINE_MAIN_FUNCTION(pep::ServiceApplication<pep::RegistrationServer>)
