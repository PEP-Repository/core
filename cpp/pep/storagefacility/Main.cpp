#include <pep/service-application/ServiceApplication.hpp>
#include <pep/storagefacility/StorageFacility.hpp>
#include <pep/application/Application.hpp> // for PEP_DEFINE_MAIN_FUNCTION

/// \brief entry point for storage facility
///
/// \param argc number of arguments
/// \param argv array of strings containing arguments
PEP_DEFINE_MAIN_FUNCTION(pep::ServiceApplication<pep::StorageFacility>)
