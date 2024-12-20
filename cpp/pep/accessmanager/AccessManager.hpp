#pragma once

#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <pep/accessmanager/AmaMessages.hpp>
#include <pep/async/WorkerPool.hpp>
#include <pep/rsk/Verifiers.hpp>
#include <pep/server/Server.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/transcryptor/KeyComponentMessages.hpp>

#include <filesystem>

namespace pep {

class AccessManager : public SigningServer {
public:
  class Backend; // Public to allow unit testing

  class Parameters : public SigningServer::Parameters {
  public:
    Parameters(std::shared_ptr<boost::asio::io_service> io_service, const Configuration& config);

    void setGlobalConfiguration(std::shared_ptr<GlobalConfiguration> gc);
    std::shared_ptr<GlobalConfiguration> getGlobalConfiguration() const;

    /*!
    * \return The pseudonym key
    */
    const ElgamalPrivateKey& getPseudonymKey() const;
    /*!
    * \param pseudonymKey The pseudonym key
    * \return The Parameters instance itself
    */
    void setPseudonymKey(const ElgamalPrivateKey& pseudonymKey);

    const ElgamalPublicKey& getPublicKeyPseudonyms() const;
    void setPublicKeyPseudonyms(const ElgamalPublicKey& pk);

    /*!
    * \return The connection to the transcryptor
    */
    std::shared_ptr<TLSMessageProtocol::Connection> getTranscryptor() const;
    /*!
    * \param transcryptor The connection to the transcryptor
    * \return The Parameters instance itself
    */
    void setTranscryptor(std::shared_ptr<TLSMessageProtocol::Connection> transcryptor);

    std::shared_ptr<PseudonymTranslator> getPseudonymTranslator() const;
    std::shared_ptr<DataTranslator> getDataTranslator() const;
    void setPseudonymTranslator(std::shared_ptr<PseudonymTranslator> pseudonymTranslator);
    void setDataTranslator(std::shared_ptr<DataTranslator> dataTranslator);


    std::shared_ptr<Backend> getBackend() const;
    void setBackend(std::shared_ptr<Backend> backend);

  protected:
    void check() override;

  private:
    std::shared_ptr<GlobalConfiguration> globalConf;
    std::optional<ElgamalPrivateKey> pseudonymKey;
    std::optional<ElgamalPublicKey> publicKeyPseudonyms;
    std::shared_ptr<TLSMessageProtocol::Connection> transcryptor;
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
  std::vector<std::string> getChecksumChainNames() const override;
  void computeChecksumChainChecksum(
    const std::string& chain, std::optional<uint64_t> maxCheckpoint,
    uint64_t& checksum, uint64_t& checkpoint) override;

private:
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleKeyComponentRequest(std::shared_ptr<SignedKeyComponentRequest> pRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleTicketRequest2(std::shared_ptr<SignedTicketRequest2> pClientRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleEncryptionKeyRequest(std::shared_ptr<SignedEncryptionKeyRequest> pClientRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleAmaMutationRequest(std::shared_ptr<SignedAmaMutationRequest> pRequest);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleAmaQuery(std::shared_ptr<SignedAmaQuery> request);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleGlobalConfigurationRequest(std::shared_ptr<GlobalConfigurationRequest> request);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleVerifiersRequest(std::shared_ptr<VerifiersRequest> request);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleColumnAccessRequest(std::shared_ptr<SignedColumnAccessRequest> request);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleParticipantGroupAccessRequest(std::shared_ptr<SignedParticipantGroupAccessRequest> request);
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handleColumnNameMappingRequest(std::shared_ptr<SignedColumnNameMappingRequest> signedRequest);

  /*!
  * A single method that performs both the adding and the removal of participants in pgroups for a given AmaMutationRequest.
  * \param amRequest a request that contains the information for the addition or removal of participants (their identifiers and groups).
  * \param performRemove: a boolean that tells wether to remove the participant from a pgroup (value false) or to add a participant to a group (value true).
  * \return An observable without an effective return value (FakeVoid)
  * \remark These methods are defined here and not in Backend due to the quick interactions with Transcryptor.
  */
  rxcpp::observable<FakeVoid> removeOrAddParticipantsInGroupsForRequest(const AmaMutationRequest& amRequest, const bool removeParticipantOp);
  rxcpp::observable<FakeVoid> addParticipantsToGroupsForRequest(const AmaMutationRequest& amRequest);
  rxcpp::observable<FakeVoid> removeParticipantsFromGroupsForRequest(const AmaMutationRequest& amRequest);


public:
  /* !
   * \brief Splits up the given columnGroups over multiple responses to make sure the response message lengths do not exceed their max size.
   * \param columnGroups: The column groups that need to be devided into multiple response messages.
   * \param maxSize: The size at which to cut up responses. For testing purposes, this can be set to a lower number. For most purposes it should be left at the default.
   * \return An observable emitting iterated AmaQueryResponses
   */
  static std::vector<AmaQueryResponse> ExtractPartialColumnGroupQueryResponse(const std::vector<AmaQRColumnGroup>& columnGroups, const size_t maxSize = TLSMessageProtocol::MAX_SIZE_OF_MESSAGE); // TODO: move out of AM's (public even!) interface

private:
  ElgamalPrivateKey mPseudonymKey;
  ElgamalPublicKey mPublicKeyPseudonyms;
  std::shared_ptr<TLSMessageProtocol::Connection> transcryptor;
  std::shared_ptr<PseudonymTranslator> mPseudonymTranslator;
  std::shared_ptr<DataTranslator> mDataTranslator;
  std::shared_ptr<Backend> backend;
  std::shared_ptr<GlobalConfiguration> globalConf;
  std::shared_ptr<Metrics> lpMetrics;
  std::shared_ptr<WorkerPool> mWorkerPool;
  uintmax_t mNextTicketRequestNumber = 1U;
};

}
