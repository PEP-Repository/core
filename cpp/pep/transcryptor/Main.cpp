#include <pep/service-application/ServiceApplication.hpp>
#include <pep/transcryptor/Transcryptor.hpp>
#include <pep/application/Application.hpp> // for PEP_DEFINE_MAIN_FUNCTION

/// \brief entry point for transcryptor
///
/// \param argc number of arguments
/// \param argv array of strings containing arguments
PEP_DEFINE_MAIN_FUNCTION(pep::ServiceApplication<pep::Transcryptor>)
