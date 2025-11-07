#pragma once

#include <string>
#include <vector>

// Provides hard-coded sample certificates that must be re-generated when (or before) they expire.
// See the .cpp file for instructions on how to re-generate them.

extern const std::string rootCACertPEM;
extern const std::string pepServerCACertPEM;
extern const std::vector<unsigned char> pepServerCACertDER;
extern const std::string pepServerCAPrivateKeyPEM;
extern const std::string pepAuthserverCertPEM;
extern const std::string pepAuthserverCertPEMExpired;
extern const std::string accessmanagerTLSCertPEM;

extern const std::string rootCACertPEMExpired;
extern const std::string pepServerCACertPEMWithExpiredRoot;
extern const std::string pepAuthserverCertPEMWithExpiredRoot;