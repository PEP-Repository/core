#pragma once

#include <pep/auth/EnrolledParty.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <pep/rsk/RskRecipient.hpp>

#include <string>

namespace pep {

RekeyRecipient RekeyRecipientForCertificate(const X509Certificate& cert);
RekeyRecipient RekeyRecipientForServer(const EnrolledParty& server);

ReshuffleRecipient PseudonymRecipientForCertificate(const X509Certificate& cert);
ReshuffleRecipient PseudonymRecipientForUserGroup(std::string userGroup);
ReshuffleRecipient PseudonymRecipientForServer(const EnrolledParty& server);

SkRecipient RecipientForCertificate(const X509Certificate& cert);
SkRecipient RecipientForServer(const EnrolledParty& server);

} // namespace pep
