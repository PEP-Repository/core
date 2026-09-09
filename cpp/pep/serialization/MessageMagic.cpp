#include <pep/serialization/MessageMagic.hpp>
#include <pep/serialization/SerializeException.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Raw.hpp>

#include <boost/bimap.hpp>
#include <xxhash.h>

namespace pep {

namespace {

using Magics = boost::bimap<MessageMagic, std::string>;

Magics::value_type MakeMagicEntry(std::string crossPlatformName) {
  auto magic = CalculateMessageMagic(crossPlatformName);
  return { magic, std::move(crossPlatformName) };
}

Magics PredefinedMagics() {
  // Lists all message type(name)s so that they can be reported by any recipient: see https://gitlab.pep.cs.ru.nl/pep/core/-/work_items/2980#note_63692
  std::vector<Magics::value_type> entries{
    MakeMagicEntry("AddUserIdentifier"),
    MakeMagicEntry("AddUserToGroup"),
    MakeMagicEntry("AdditionalStickerDefinition"),
    MakeMagicEntry("AmaAddColumnToGroup"),
    MakeMagicEntry("AmaAddParticipantToGroup"),
    MakeMagicEntry("AmaCreateColumn"),
    MakeMagicEntry("AmaCreateColumnGroup"),
    MakeMagicEntry("AmaCreateColumnGroupAccessRule"),
    MakeMagicEntry("AmaCreateParticipantGroup"),
    MakeMagicEntry("AmaCreateParticipantGroupAccessRule"),
    MakeMagicEntry("AmaMutationRequest"),
    MakeMagicEntry("AmaMutationResponse"),
    MakeMagicEntry("AmaQRColumn"),
    MakeMagicEntry("AmaQRColumnGroup"),
    MakeMagicEntry("AmaQRColumnGroupAccessRule"),
    MakeMagicEntry("AmaQRParticipantGroup"),
    MakeMagicEntry("AmaQRParticipantGroupAccessRule"),
    MakeMagicEntry("AmaQuery"),
    MakeMagicEntry("AmaQueryResponse"),
    MakeMagicEntry("AmaRemoveColumn"),
    MakeMagicEntry("AmaRemoveColumnFromGroup"),
    MakeMagicEntry("AmaRemoveColumnGroup"),
    MakeMagicEntry("AmaRemoveColumnGroupAccessRule"),
    MakeMagicEntry("AmaRemoveParticipantFromGroup"),
    MakeMagicEntry("AmaRemoveParticipantGroup"),
    MakeMagicEntry("AmaRemoveParticipantGroupAccessRule"),
    MakeMagicEntry("AssessorDefinition"),
    MakeMagicEntry("Bytes"),
    MakeMagicEntry("CastorShortPseudonymDefinition"),
    MakeMagicEntry("CastorStorageDefinition"),
    MakeMagicEntry("CertificateReplacementCommitRequest"),
    MakeMagicEntry("CertificateReplacementCommitResponse"),
    MakeMagicEntry("CertificateReplacementRequest"),
    MakeMagicEntry("CertificateReplacementResponse"),
    MakeMagicEntry("ChecksumChainNamesRequest"),
    MakeMagicEntry("ChecksumChainNamesResponse"),
    MakeMagicEntry("ChecksumChainRequest"),
    MakeMagicEntry("ChecksumChainResponse"),
    MakeMagicEntry("ColumnAccessRequest"),
    MakeMagicEntry("ColumnAccessResponse"),
    MakeMagicEntry("ColumnGroupAccess"),
    MakeMagicEntry("ColumnNameMapping"),
    MakeMagicEntry("ColumnNameMappingRequest"),
    MakeMagicEntry("ColumnNameMappingResponse"),
    MakeMagicEntry("ColumnNameSection"),
    MakeMagicEntry("ColumnSpecification"),
    MakeMagicEntry("ConfigVersion"),
    MakeMagicEntry("CreateUser"),
    MakeMagicEntry("CreateUserGroup"),
    MakeMagicEntry("CsrRequest"),
    MakeMagicEntry("CsrResponse"),
    MakeMagicEntry("CurvePoint"),
    MakeMagicEntry("CurveScalar"),
    MakeMagicEntry("DataDeleteRequest2"),
    MakeMagicEntry("DataDeleteResponse2"),
    MakeMagicEntry("DataEnumerationEntry2"),
    MakeMagicEntry("DataEnumerationRequest2"),
    MakeMagicEntry("DataEnumerationResponse2"),
    MakeMagicEntry("DataHistoryEntry2"),
    MakeMagicEntry("DataHistoryRequest2"),
    MakeMagicEntry("DataHistoryResponse2"),
    MakeMagicEntry("DataPayloadPage"),
    MakeMagicEntry("DataReadRequest2"),
    MakeMagicEntry("DataRequestEntry2"),
    MakeMagicEntry("DataSizeRequest"),
    MakeMagicEntry("DataSizeResponse"),
    MakeMagicEntry("DataStoreEntry2"),
    MakeMagicEntry("DataStoreRequest2"),
    MakeMagicEntry("DataStoreResponse2"),
    MakeMagicEntry("DeviceRegistrationDefinition"),
    MakeMagicEntry("ElgamalEncryption"),
    MakeMagicEntry("EncryptedBytes"),
    MakeMagicEntry("EncryptedSFId"),
    MakeMagicEntry("EncryptionKeyRequest"),
    MakeMagicEntry("EncryptionKeyResponse"),
    MakeMagicEntry("EnrollmentRequest"),
    MakeMagicEntry("EnrollmentResponse"),
    MakeMagicEntry("Error"),
    MakeMagicEntry("FindUserRequest"),
    MakeMagicEntry("FindUserResponse"),
    MakeMagicEntry("GenerablePseudonymFormat"),
    MakeMagicEntry("GlobalConfiguration"),
    MakeMagicEntry("GlobalConfigurationRequest"),
    MakeMagicEntry("IndexList"),
    MakeMagicEntry("IndexedTicket2"),
    MakeMagicEntry("KeyComponentRequest"),
    MakeMagicEntry("KeyComponentResponse"),
    MakeMagicEntry("KeyRequestEntry"),
    MakeMagicEntry("ListCastorImportColumnsRequest"),
    MakeMagicEntry("ListCastorImportColumnsResponse"),
    MakeMagicEntry("LocalPseudonyms"),
    MakeMagicEntry("LogIssuedTicketRequest"),
    MakeMagicEntry("LogIssuedTicketResponse"),
    MakeMagicEntry("Metadata"),
    MakeMagicEntry("MetadataReadRequest2"),
    MakeMagicEntry("MetadataUpdateRequest2"),
    MakeMagicEntry("MetadataUpdateResponse2"),
    MakeMagicEntry("MetadataXEntry"),
    MakeMagicEntry("MetricsRequest"),
    MakeMagicEntry("MetricsResponse"),
    MakeMagicEntry("MigrateUserDbToAccessManagerRequest"),
    MakeMagicEntry("MigrateUserDbToAccessManagerResponse"),
    MakeMagicEntry("ModifyUserGroup"),
    MakeMagicEntry("NamedMetadataXEntry"),
    MakeMagicEntry("PEPIdRegistrationRequest"),
    MakeMagicEntry("PEPIdRegistrationResponse"),
    MakeMagicEntry("PagePathRequest"),
    MakeMagicEntry("PagePathResponse"),
    MakeMagicEntry("ParticipantGroupAccess"),
    MakeMagicEntry("ParticipantGroupAccessRequest"),
    MakeMagicEntry("ParticipantGroupAccessResponse"),
    MakeMagicEntry("PingRequest"),
    MakeMagicEntry("PingResponse"),
    MakeMagicEntry("PseudonymFormat"),
    MakeMagicEntry("QRUser"),
    MakeMagicEntry("QRUserGroupMembership"),
    MakeMagicEntry("RegexPseudonymFormat"),
    MakeMagicEntry("RegistrationRequest"),
    MakeMagicEntry("RegistrationResponse"),
    MakeMagicEntry("RekeyRequest"),
    MakeMagicEntry("RekeyResponse"),
    MakeMagicEntry("RemoveUser"),
    MakeMagicEntry("RemoveUserFromGroup"),
    MakeMagicEntry("RemoveUserGroup"),
    MakeMagicEntry("RemoveUserIdentifier"),
    MakeMagicEntry("ReshuffleRekeyVerifiers"),
    MakeMagicEntry("ReshuffleRekeyVerifiersProof"),
    MakeMagicEntry("RskProof"),
    MakeMagicEntry("SFId"),
    MakeMagicEntry("ScalarMultProof"),
    MakeMagicEntry("ServerVerifiers"),
    MakeMagicEntry("SetStructureMetadataRequest"),
    MakeMagicEntry("SetStructureMetadataResponse"),
    MakeMagicEntry("ShortPseudonymDefinition"),
    MakeMagicEntry("ShortPseudonymErratum"),
    MakeMagicEntry("Signature"),
    MakeMagicEntry("SignedAmaMutationRequest"),
    MakeMagicEntry("SignedAmaQuery"),
    MakeMagicEntry("SignedCertificateReplacementCommitRequest"),
    MakeMagicEntry("SignedCertificateReplacementRequest"),
    MakeMagicEntry("SignedCertificateReplacementResponse"),
    MakeMagicEntry("SignedChecksumChainNamesRequest"),
    MakeMagicEntry("SignedChecksumChainRequest"),
    MakeMagicEntry("SignedColumnAccessRequest"),
    MakeMagicEntry("SignedColumnNameMappingRequest"),
    MakeMagicEntry("SignedCsrRequest"),
    MakeMagicEntry("SignedCsrResponse"),
    MakeMagicEntry("SignedDataDeleteRequest2"),
    MakeMagicEntry("SignedDataEnumerationRequest2"),
    MakeMagicEntry("SignedDataHistoryRequest2"),
    MakeMagicEntry("SignedDataReadRequest2"),
    MakeMagicEntry("SignedDataSizeRequest"),
    MakeMagicEntry("SignedDataStoreRequest2"),
    MakeMagicEntry("SignedEncryptionKeyRequest"),
    MakeMagicEntry("SignedFindUserRequest"),
    MakeMagicEntry("SignedKeyComponentRequest"),
    MakeMagicEntry("SignedMetadataReadRequest2"),
    MakeMagicEntry("SignedMetadataUpdateRequest2"),
    MakeMagicEntry("SignedMetricsRequest"),
    MakeMagicEntry("SignedMigrateUserDbToAccessManagerRequest"),
    MakeMagicEntry("SignedPEPIdRegistrationRequest"),
    MakeMagicEntry("SignedPagePathRequest"),
    MakeMagicEntry("SignedParticipantGroupAccessRequest"),
    MakeMagicEntry("SignedPingResponse"),
    MakeMagicEntry("SignedRegistrationRequest"),
    MakeMagicEntry("SignedSetStructureMetadataRequest"),
    MakeMagicEntry("SignedStructureMetadataRequest"),
    MakeMagicEntry("SignedTicket2"),
    MakeMagicEntry("SignedTicketRequest2"),
    MakeMagicEntry("SignedTokenBlockingCreateRequest"),
    MakeMagicEntry("SignedTokenBlockingListRequest"),
    MakeMagicEntry("SignedTokenBlockingRemoveRequest"),
    MakeMagicEntry("SignedTokenRequest"),
    MakeMagicEntry("SignedUserMutationRequest"),
    MakeMagicEntry("SignedUserQuery"),
    MakeMagicEntry("StructureMetadataEntry"),
    MakeMagicEntry("StructureMetadataKey"),
    MakeMagicEntry("StructureMetadataRequest"),
    MakeMagicEntry("StructureMetadataSubjectKey"),
    MakeMagicEntry("StudyContext"),
    MakeMagicEntry("Ticket2"),
    MakeMagicEntry("TicketRequest2"),
    MakeMagicEntry("Timestamp"),
    MakeMagicEntry("TokenBlockingBlocklistEntry"),
    MakeMagicEntry("TokenBlockingCreateRequest"),
    MakeMagicEntry("TokenBlockingCreateResponse"),
    MakeMagicEntry("TokenBlockingListRequest"),
    MakeMagicEntry("TokenBlockingListResponse"),
    MakeMagicEntry("TokenBlockingRemoveRequest"),
    MakeMagicEntry("TokenBlockingRemoveResponse"),
    MakeMagicEntry("TokenBlockingTokenIdentifier"),
    MakeMagicEntry("TokenRequest"),
    MakeMagicEntry("TokenResponse"),
    MakeMagicEntry("TranscryptorRequest"),
    MakeMagicEntry("TranscryptorRequestEntries"),
    MakeMagicEntry("TranscryptorRequestEntry"),
    MakeMagicEntry("TranscryptorResponse"),
    MakeMagicEntry("UpdateExpiration"),
    MakeMagicEntry("UserGroup"),
    MakeMagicEntry("UserMutationRequest"),
    MakeMagicEntry("UserMutationResponse"),
    MakeMagicEntry("UserPseudonymFormat"),
    MakeMagicEntry("UserQuery"),
    MakeMagicEntry("UserQueryResponse"),
    MakeMagicEntry("UserVerifiersRequest"),
    MakeMagicEntry("UserVerifiersResponse"),
    MakeMagicEntry("VerifiersRequest"),
    MakeMagicEntry("VerifiersResponse"),
    MakeMagicEntry("VersionRequest"),
    MakeMagicEntry("VersionResponse"),
    MakeMagicEntry("X509Certificate"),
    MakeMagicEntry("X509CertificateChain"),
    MakeMagicEntry("X509CertificateSigningRequest"),
  };

  return Magics(entries.begin(), entries.end());
}

Magics& RegisteredMagics() {
  static auto result = PredefinedMagics();
  return result;
}

}

std::string DescribeMessageMagic(MessageMagic magic) {
  return BasicMessageMagician::DescribeMessageMagic(magic)
    .value_or("<UNKNOWN MESSAGE TYPE: " + std::to_string(magic) + ">");
}

std::string DescribeMessageMagic(std::string_view str) {
  return DescribeMessageMagic(GetMessageMagic(str));
}

MessageMagic CalculateMessageMagic(std::string_view crossPlatformName) {
  return XXH32(crossPlatformName.data(), crossPlatformName.length(), 0xcafebabe);
}

MessageMagic GetMessageMagic(std::string_view str) {
  // Make sure input is long enough to at least read the magic
  if (str.length() < sizeof(MessageMagic)) {
    PEP_LOG("GetMessageMagic", Severity::Warning) << "Received a message which is shorter than " << sizeof(MessageMagic) << " bytes";
    throw SerializeException("Invalid message: too short");
  }
  static_assert(sizeof(MessageMagic) == sizeof(std::uint32_t));
  return UnpackUint32BE(str);
}

MessageMagic PopMessageMagic(std::string& str) {
  auto magic = GetMessageMagic(str);
  str.erase(0,sizeof(MessageMagic));
  return magic;
}

MessageMagic BasicMessageMagician::EnsureRegistered(const std::string& crossPlatformName) {
  auto& magics = RegisteredMagics();
  auto pos = magics.right.find(crossPlatformName);
  if (pos != magics.right.end()) [[likely]] {
    return pos->second;
  }

  // Remind developer to add newly introduced message types to the list of PredefinedMagics (above).
  // Log before asserting so that output contains the name before flunking the process.
  PEP_LOG("BasicMessageMagician", Severity::Warning) << "Missing predefined message magic for the " << crossPlatformName << " type";
  assert(false && "Add this crossPlatformName to the 'PredefinedMagics' function");

  // Fallback in case this crossPlatformName still wasn't included in the PredefinedMagics
  auto entry = MakeMagicEntry(crossPlatformName);
  auto result = entry.left;
  magics.insert(std::move(entry));
  return result;
}

std::string_view BasicMessageMagician::SkipMessageMagic(std::string_view szMessage, MessageMagic requiredMagic) {
  auto dwObjectMagic = GetMessageMagic(szMessage);
  if (dwObjectMagic != requiredMagic) {
    PEP_LOG("BasicMessageMagician::SkipMessageMagic", Severity::Error) << "Unknown object magic " << dwObjectMagic;
    throw SerializeException("Error parsing message");
  }
  return szMessage.substr(sizeof(MessageMagic));
}

void BasicMessageMagician::WriteMagicTo(std::ostream& destination, MessageMagic magic) {
  WriteBinary(destination, magic);
}

std::optional<std::string> BasicMessageMagician::DescribeMessageMagic(MessageMagic magic) {
  const auto& magics = RegisteredMagics();
  auto pos = magics.left.find(magic);
  if (pos == magics.left.end()) {
    return std::nullopt;
  }
  return pos->second;
}

}
