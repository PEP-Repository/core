#pragma once

#include <pep/client/Client_fwd.hpp>

#include <pep/authserver/AuthClient.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/content/ParticipantPersonalia.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/registrationserver/RegistrationServerMessages.hpp>
#include <pep/keyserver/KeyClient.hpp>

namespace pep {

class Client : public CoreClient {
public:
  class Builder : public CoreClient::Builder {
  public:
    const AsymmetricKey& getPublicKeyShadowAdministration() const {
      return publicKeyShadowAdministration;
    }
    Builder& setPublicKeyShadowAdministration(const AsymmetricKey& publicKeyShadowAdministration) {
      Builder::publicKeyShadowAdministration = publicKeyShadowAdministration;
      return *this;
    }

    const EndPoint& getKeyServerEndPoint() const {
      return keyServerEndPoint;
    }
    Builder& setKeyServerEndPoint(const EndPoint& keyServerEndPoint) {
      Builder::keyServerEndPoint = keyServerEndPoint;
      return *this;
    }

    const EndPoint& getAuthserverEndPoint() const {
      return authserverEndPoint;
    }
    Builder& setAuthserverEndPoint(const EndPoint& authserverEndPoint) {
      Builder::authserverEndPoint = authserverEndPoint;
      return *this;
    }

    const EndPoint& getRegistrationServerEndPoint() const {
      return registrationServerEndPoint;
    }
    Builder& setRegistrationServerEndPoint(const EndPoint& registrationServerEndPoint) {
      Builder::registrationServerEndPoint = registrationServerEndPoint;
      return *this;
    }

    void initialize(const Configuration& config,
                    std::shared_ptr<boost::asio::io_context> io_context,
                    bool persistKeysFile);

    std::shared_ptr<Client> build() const {
      return std::shared_ptr<Client>(new Client(*this));
    }

  private:
    AsymmetricKey publicKeyShadowAdministration;
    EndPoint keyServerEndPoint;
    EndPoint authserverEndPoint;
    EndPoint registrationServerEndPoint;
  };

  /*!
   * \brief Register a participant, storing the provided personal details in PEP
   *
   * \param personalia The participant's personally identifying information.
   * \param isTestParticipant Whether the participant should be considered a test participant.
   * \param studyContext The study context the participant should be assigned to. Leave empty to assign to the default
   * context. \param complete Whether to complete the participant('s short pseudonym) registration. \return
   * rxcpp::observable< std::string > producing the (generated) participant ID
   */
  rxcpp::observable<std::string> registerParticipant(const ParticipantPersonalia& personalia,
                                                     bool isTestParticipant,
                                                     const std::string& studyContext = "",
                                                     bool complete = true);

  /*!
   * \brief Generate and store a PEP ID
   * \return rxcpp::observable< std::string > producing the (generated) participant ID
   */
  rxcpp::observable<std::string> generatePEPID();

  /*!
   * \brief Completes a participant's registration. Should be called for participants whose initial registration was
   * (possibly) incomplete, i.e. registerParticipant was called with complete == false, or the participant has been
   * registered from an earlier code base, or additional short pseudonyms need to be generated.
   *
   * \param skipIdentifierStorage Pass true if you're sure the participant ID has already been stored, i.e. this method
   * is called after a call to registerParticipant. \return rxcpp::observable< RegistrationResponse >
   */
  rxcpp::observable<std::shared_ptr<RegistrationResponse>> completeParticipantRegistration(
      const std::string& identifier, bool skipIdentifierStorage = false);

  rxcpp::observable<std::string> listCastorImportColumns(const std::string& spColumnName,
                                                         const std::optional<unsigned>& answerSetCount);

  /*!
   * \brief Enroll a user. A key pair is generated and, using a provided OAuth token, a certificate and PEP key
   * components are requested. If the enrollment is successful, the following variables are update: privateKey,
   * certificateChain, encryptionKey, publicKeyData, publicKeyPseudonyms
   *
   * \param oauthToken An OAuth token that can be used to authenticate against the key server
   * \return rxcpp::observable< EnrollmentResult >
   */
  rxcpp::observable<EnrollmentResult> enrollUser(const std::string& oauthToken);

  rxcpp::observable<std::string> requestToken(std::string subject, std::string group, Timestamp expirationTime);

  std::shared_ptr<const KeyClient> getKeyClient(bool require = true) const;
  std::shared_ptr<const AuthClient> getAuthClient(bool require = true) const;

  rxcpp::observable<ConnectionStatus> getRegistrationServerStatus();
  rxcpp::observable<VersionResponse> getRegistrationServerVersion();
  rxcpp::observable<SignedPingResponse> pingRegistrationServer() const;
  rxcpp::observable<MetricsResponse> getRegistrationServerMetrics();

  rxcpp::observable<FakeVoid> shutdown() override;

  static std::shared_ptr<Client> OpenClient(const Configuration& config,
    std::shared_ptr<boost::asio::io_context> io_context,
    bool persistKeysFile = DEFAULT_PERSIST_KEYS_FILE);

private:
  const AsymmetricKey publicKeyShadowAdministration;
  const EndPoint keyServerEndPoint;
  const EndPoint authserverEndPoint;
  const EndPoint registrationServerEndPoint;

  std::shared_ptr<KeyClient> clientKeyServer;
  std::shared_ptr<AuthClient> clientAuthserver;
  std::shared_ptr<messaging::ServerConnection> clientRegistrationServer;

  Client(const Builder& builder);

  rxcpp::observable<std::shared_ptr<RegistrationResponse>> generateShortPseudonyms(const PolymorphicPseudonym& pp,
                                                                                   const std::string& identifier);
};

}
