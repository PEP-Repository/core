#pragma once

#include <pep/client/Client_fwd.hpp>

#include <pep/authserver/AuthServerProxy.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/content/ParticipantPersonalia.hpp>
#include <pep/keyserver/KeyServerProxy.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/registrationserver/RegistrationServerProxy.hpp>

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
   * \brief Completes a participant's registration. Should be called for participants whose initial registration was
   * (possibly) incomplete, i.e. registerParticipant was called with complete == false, or the participant has been
   * registered from an earlier code base, or additional short pseudonyms need to be generated.
   *
   * \param skipIdentifierStorage Pass true if you're sure the participant ID has already been stored, i.e. this method
   * is called after a call to registerParticipant. \return rxcpp::observable< RegistrationResponse >
   */
  rxcpp::observable<FakeVoid> completeParticipantRegistration(
      const std::string& identifier, bool skipIdentifierStorage = false);

  /*!
   * \brief Enroll a user. A key pair is generated and, using a provided OAuth token, a certificate and PEP key
   * components are requested. If the enrollment is successful, the following variables are update: privateKey,
   * certificateChain, encryptionKey, publicKeyData, publicKeyPseudonyms
   *
   * \param oauthToken An OAuth token that can be used to authenticate against the key server
   * \return rxcpp::observable< EnrollmentResult >
   */
  rxcpp::observable<EnrollmentResult> enrollUser(const std::string& oauthToken);

  std::shared_ptr<const KeyServerProxy> getKeyServerProxy(bool require = true) const;
  std::shared_ptr<const AuthServerProxy> getAuthServerProxy(bool require = true) const;
  std::shared_ptr<const RegistrationServerProxy> getRegistrationServerProxy(bool require = true) const;

  rxcpp::observable<FakeVoid> shutdown() override;

  static std::shared_ptr<Client> OpenClient(const Configuration& config,
    std::shared_ptr<boost::asio::io_context> io_context,
    bool persistKeysFile = DEFAULT_PERSIST_KEYS_FILE);

private:
  const AsymmetricKey publicKeyShadowAdministration;
  const EndPoint keyServerEndPoint;
  const EndPoint authserverEndPoint;
  const EndPoint registrationServerEndPoint;

  std::shared_ptr<KeyServerProxy> keyServerProxy;
  std::shared_ptr<AuthServerProxy> authServerProxy;
  std::shared_ptr<RegistrationServerProxy> registrationServerProxy;

  Client(const Builder& builder);

  rxcpp::observable<FakeVoid> generateShortPseudonyms(PolymorphicPseudonym pp, const std::string& identifier);
  rxcpp::observable<std::string> getInaccessibleColumns(const std::string& mode, rxcpp::observable<std::string> columns);
};

}
