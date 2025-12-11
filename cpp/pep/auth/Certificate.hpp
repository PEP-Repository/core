#pragma once

#include <pep/crypto/X509Certificate.hpp>

namespace pep {

bool IsServerTlsCertificate(const X509Certificate& certificate);
bool IsServerSigningCertificate(const X509Certificate& certificate);
bool IsUserSigningCertificate(const X509Certificate& certificate);

std::optional<std::string> GetSubjectIfServerSigningCertificate(const X509Certificate& certificate);
std::optional<std::string> GetSubjectIfServerSigningCertificate(const X509CertificateChain& chain);

}
