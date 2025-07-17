#pragma once

#include <pep/auth/FacilityType.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/rsk/RskRecipient.hpp>

#include <string>

namespace pep {

RekeyRecipient RekeyRecipientForCertificate(const X509Certificate& cert);
RekeyRecipient RekeyRecipientForServer(const FacilityType& server);

ReshuffleRecipient PseudonymRecipientForCertificate(const X509Certificate& cert);
ReshuffleRecipient PseudonymRecipientForUserGroup(std::string userGroup);
ReshuffleRecipient PseudonymRecipientForServer(const FacilityType& server);

SkRecipient RecipientForCertificate(const X509Certificate& cert);
SkRecipient RecipientForServer(const FacilityType& server);

} // namespace pep
