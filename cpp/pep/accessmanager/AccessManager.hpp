#pragma once

#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <pep/accessmanager/AmaMessages.hpp>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/async/WorkerPool.hpp>
#include <pep/keyserver/KeyServerProxy.hpp>
#include <pep/rsk/Verifiers.hpp>
#include <pep/server/SigningServer.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/transcryptor/TranscryptorProxy.hpp>

#include <filesystem>

namespace pep {

class AccessManager : public SigningServer {
public:
  class Backend; // Public to allow unit testing

  class Parameters : public SigningServer::Parameters {
  protected:
    EnrolledParty enrollsAs() const noexcept override { return EnrolledParty::AccessManager; }

  public:
    Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

    void setGlobalConfiguration(std::shared_ptr<GlobalConfiguration> gc);
    std::shared_ptr<GlobalConfiguration> getGlobalConfiguration() const;

    /*!
    * \return The pseudonym key
    */
    const ElgamalPrivateKey& getPseudonymKey() const;
    /*!
    * \param pseudonymKey The pseudonym key
    */
    void setPseudonymKey(const ElgamalPrivateKey& pseudonymKey);

    const ElgamalPublicKey& getPublicKeyPseudonyms() const;
    void setPublicKeyPseudonyms(const ElgamalPublicKey& pk);

    /*!
    * \return The endpoint of the transcryptor
    */
    const EndPoint& getTranscryptorEndPoint() const;

    /*!
    * \return The endpoint of the keyserver
    */
    const EndPoint& getKeyServerEndPoint() const;

    std::shared_ptr<PseudonymTranslator> getPseudonymTranslator() const;
    std::shared_ptr<DataTranslator> getDataTranslator() const;
    void setPseudonymTranslator(std::shared_ptr<PseudonymTranslator> pseudonymTranslator);
    void setDataTranslator(std::shared_ptr<DataTranslator> dataTranslator);


    std::shared_ptr<Backend> getBackend() const;
    void setBackend(std::shared_ptr<Backend> backend);

  protected:
    void check() const override;

  private:
    std::shared_ptr<GlobalConfiguration> globalConf;
    std::optional<ElgamalPrivateKey> pseudonymKey;
    std::optional<ElgamalPublicKey> publicKeyPseudonyms;
    EndPoint transcryptorEndPoint;
    EndPoint keyServerEndPoint;
    std::shared_ptr<PseudonymTranslator> pseudonymTranslator;
    std::shared_ptr<DataTranslator> dataTranslator;
    std::shared_ptr<Backend> backend;
  };
private:

  class Metrics : public RegisteredMetrics {
  public:
    Metrics(std::shared_ptr<prometheus::Registry> registry);

    prometheus::Summary& enckey_request_duration;
    prometheus::Summary& ticket_request2_duration;
    prometheus::Summary& keyComponent_request_duration;
    prometheus::Summary& ticket_request_duration;
  };




public:
  explicit AccessManager(std::shared_ptr<Parameters> parameters);

protected:
  std::string describe() const override;
  std::optional<std::filesystem::path> getStoragePath() override;
  std::unordered_set<std::string> getAllowedChecksumChainRequesters() override;
  std::vector<std::string> getChecksumChainNames() const override;
  void computeChecksumChainChecksum(
    const std::string& chain, std::optional<uint64_t> maxCheckpoint,
    uint64_t& checksum, uint64_t& checkpoint) override;

private:
  messaging::MessageBatches handleKeyComponentRequest(std::shared_ptr<SignedKeyComponentRequest> pRequest);
  messaging::MessageBatches handleTicketRequest2(std::shared_ptr<SignedTicketRequest2> pClientRequest);
  messaging::MessageBatches handleEncryptionKeyRequest(std::shared_ptr<SignedEncryptionKeyRequest> pClientRequest);
  messaging::MessageBatches handleAmaMutationRequest(std::shared_ptr<SignedAmaMutationRequest> pRequest);
  messaging::MessageBatches handleAmaQuery(std::shared_ptr<SignedAmaQuery> request);
  messaging::MessageBatches handleUserQuery(std::shared_ptr<SignedUserQuery> signedRequest);
  messaging::MessageBatches handleUserMutationRequest(std::shared_ptr<SignedUserMutationRequest> signedRequest);
  messaging::MessageBatches handleGlobalConfigurationRequest(std::shared_ptr<GlobalConfigurationRequest> request);
  messaging::MessageBatches handleVerifiersRequest(std::shared_ptr<VerifiersRequest> request);
  messaging::MessageBatches handleColumnAccessRequest(std::shared_ptr<SignedColumnAccessRequest> request);
  messaging::MessageBatches handleParticipantGroupAccessRequest(std::shared_ptr<SignedParticipantGroupAccessRequest> request);
  messaging::MessageBatches handleColumnNameMappingRequest(std::shared_ptr<SignedColumnNameMappingRequest> signedRequest);
  messaging::MessageBatches handleMigrateUserDbToAccessManagerRequest(std::shared_ptr<SignedMigrateUserDbToAccessManagerRequest> signedRequest, messaging::MessageSequence chunksObservable);
  messaging::MessageBatches handleFindUserRequest(std::shared_ptr<SignedFindUserRequest> signedRequest);

  messaging::MessageBatches handleStructureMetadataRequest(std::shared_ptr<SignedStructureMetadataRequest> request);
  messaging::MessageBatches handleSetStructureMetadataRequest(std::shared_ptr<SignedSetStructureMetadataRequest> request, messaging::MessageSequence chunks);

  /*!
  * A single method that performs both the adding and the removal of participants in pgroups for a given AmaMutationRequest.
  * \param amRequest a request that contains the information for the addition or removal of participants (their identifiers and groups).
  * \param performRemove a boolean that tells wether to remove the participant from a pgroup (value true) or to add a participant to a group (value false).
  * \return An observable without an effective return value (FakeVoid)
  * \remark These methods are defined here and not in Backend due to the quick interactions with Transcryptor.
  */
  rxcpp::observable<FakeVoid> removeOrAddParticipantsInGroupsForRequest(const AmaMutationRequest& amRequest, bool performRemove);
  rxcpp::observable<FakeVoid> addParticipantsToGroupsForRequest(const AmaMutationRequest& amRequest);
  rxcpp::observable<FakeVoid> removeParticipantsFromGroupsForRequest(const AmaMutationRequest& amRequest);


public:
  /* !
   * \brief Splits up the given columnGroups over multiple responses to make sure the response message lengths do not exceed their max size.
   * \param columnGroups: The column groups that need to be devided into multiple response messages.
   * \param maxSize: The size at which to cut up responses. For testing purposes, this can be set to a lower number. For most purposes it should be left at the default.
   * \return An observable emitting iterated AmaQueryResponses
   */
  static std::vector<AmaQueryResponse> ExtractPartialColumnGroupQueryResponse(const std::vector<AmaQRColumnGroup>& columnGroups, const size_t maxSize = messaging::MAX_SIZE_OF_MESSAGE); // TODO: move out of AM's (public even!) interface

private:
  ElgamalPrivateKey mPseudonymKey;
  ElgamalPublicKey mPublicKeyPseudonyms;
  TranscryptorProxy mTranscryptorProxy;
  KeyServerProxy mKeyServerProxy;
  std::shared_ptr<PseudonymTranslator> mPseudonymTranslator;
  std::shared_ptr<DataTranslator> mDataTranslator;
  std::shared_ptr<Backend> backend;
  std::shared_ptr<GlobalConfiguration> globalConf;
  std::shared_ptr<Metrics> lpMetrics;
  std::shared_ptr<WorkerPool> mWorkerPool;
  uintmax_t mNextTicketRequestNumber = 1U;
};

}
