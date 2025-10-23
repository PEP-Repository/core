#pragma once

#include <pep/core-client/CoreClient_fwd.hpp>

#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <pep/accessmanager/AmaMessages.hpp>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/async/FakeVoid.hpp>
#include <pep/async/WorkerPool.hpp>
#include <pep/crypto/Timestamp.hpp>
#include <pep/elgamal/CurvePoint.hpp>
#include <pep/messaging/ConnectionStatus.hpp>
#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/messaging/MessageSequence.hpp>
#include <pep/messaging/ServerConnection.hpp>
#include <pep/networking/EndPoint.hpp>
#include <pep/rsk/Verifiers.hpp>
#include <pep/server/MonitoringMessages.hpp>
#include <pep/storagefacility/StorageFacilityMessages.hpp>
#include <pep/structure/ColumnName.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/structure/StudyContext.hpp>
#include <pep/transcryptor/KeyComponentMessages.hpp>
#include <pep/utils/Configuration_fwd.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>
#include <filesystem>
#include <optional>
#include <type_traits>

#include <pep/async/IoContext_fwd.hpp>
#include <boost/core/noncopyable.hpp>
#include <rxcpp/rx-lite.hpp>


namespace pep {

struct EnrollmentResult {
  ElgamalPrivateKey privateKeyData;
  ElgamalPrivateKey privateKeyPseudonyms;
  AsymmetricKey privateKey;
  X509CertificateChain certificateChain;

  void writeJsonTo(std::ostream& os, bool writeDataKey = true, bool writePrivateKey = true, bool writeCertificateChain = true) const;
};

struct DataStorageResult2 {
  std::vector<std::string> mIds;
};

// Represents a cell denotation returned by a CoreClient method
struct DataCellResult {
  // Encrypted local pseudonyms belonging to the polymorphic pseudonym to which
  // the file belongs.  The encrypted "row identifier".
  std::shared_ptr<LocalPseudonyms> mLocalPseudonyms;

  // Can be used to match equal mLocalPseudonyms (without having to
  // compare mLocalPseudonyms) within the result of a single call
  // to a CoreClient method that produces DataCellResult instances.
  uint32_t mLocalPseudonymsIndex{};

  std::string mColumn; // column to which this file belongs

  // The decrypted local pseudonym for the access group of the client
  // belonging to the PP of this file.  This is the "row identifier"
  // of this file with respect to our access group.
  // This field is only set if includeAccessGroupPseudonyms was set.
  std::shared_ptr<LocalPseudonym> mAccessGroupPseudonym;
};

/// Represents a file with metadata but without content
struct EnumerateResult : public DataCellResult {
  /// Partially encrypted metadata of the file
  Metadata mMetadata;

  /// Encrypted key to decrypt the data
  EncryptedKey mPolymorphicKey;

  /// Size of file on storage facility, in bytes
  /// Note that this is both the size of the encrypted ciphertext as its plaintext alternative
  uint64_t mFileSize{};

  /// Storage facility identifier
  std::string mId;
};

struct RetrieveResult {
  /// Index of the file this result belongs to
  uint32_t mIndex{};

  /// Decrypted metadata of the file
  Metadata mMetadataDecrypted;

  /// Content of the file, if requested
  std::optional<rxcpp::observable<std::string>> mContent;
};

// Represents a file retrieved using enumerateAndRetrieveData2.
struct EnumerateAndRetrieveResult : public EnumerateResult {
  std::string mData;    // contents of the file

  // metadata of the file with the decrypted metadata entries - set only
  // when mDataSet is true.
  std::optional<Metadata> mMetadataDecrypted;

  // If a dataSizeLimit was specified, mData might not be set.  This
  // field indicates whether mData was set.
  bool mDataSet{};
};

// Result of a getHistory2 or deleteData2 call
struct HistoryResult : public DataCellResult {
  Timestamp mTimestamp;
  std::optional<std::string> mId{};
};

// Used as parameter to CoreClient::deleteData2
struct Storage2Entry {
  Storage2Entry(
    std::shared_ptr<PolymorphicPseudonym> pp,
    std::string column) :
    mColumn(std::move(column)),
    mPolymorphicPseudonym(std::move(pp)) { }

  // Column of the storage location.
  std::string mColumn;

  // Polymorphic pseudonym of the storage location.
  std::shared_ptr<PolymorphicPseudonym> mPolymorphicPseudonym;

  // Request to overwrite timestamp.
  // XXX This is a temporary field and will be removed.
  std::optional<Timestamp> mTimestamp;
};

struct StoreMetadata2Entry : public Storage2Entry {
  StoreMetadata2Entry(
    std::shared_ptr<PolymorphicPseudonym> pp,
    std::string column) :
    Storage2Entry(std::move(pp), std::move(column)) {}

  // Extra metadata entries. The payload of the MetadataXEntry-s with
  // encrypted=true will be encrypted by the storeData2 or
  // updateMetadata2 method.
  std::map<std::string, MetadataXEntry> mXMetadata;
};

// Used as parameter to CoreClient::storeData2
struct StoreData2Entry : public StoreMetadata2Entry {
  StoreData2Entry(
    std::shared_ptr<PolymorphicPseudonym> pp,
    std::string column,
    messaging::MessageBatches batches) :
      StoreMetadata2Entry(pp, column),
      mBatches(batches) { }

  StoreData2Entry(
      std::shared_ptr<PolymorphicPseudonym> pp,
      std::string column,
      std::shared_ptr<std::string> data,
      const std::vector<NamedMetadataXEntry>& xentries = {});

  // The data to store should be provided as a rx stream^2 of strings (^2 due to have control over when stuff is send)
  messaging::MessageBatches mBatches;
};

// Used as arguments for CoreClient::requestTicket2
struct requestTicket2Opts{
  std::vector<std::string> participantGroups;
  std::vector<PolymorphicPseudonym> pps;
  std::vector<std::string> columnGroups;
  std::vector<std::string> columns;
  std::vector<std::string> modes; // "read-meta", "write-meta", "read", "write"

  // If set, check whether this ticket has at least the desired scope.
  // If it does, returns this ticket.  Otherwise, request a new one
  // (unless forceTicket is set).
  std::shared_ptr<IndexedTicket2> ticket = nullptr;

  // If set, simply checks whether the given ticket has at least the given
  // scope and returns an error if it doesn't.
  bool forceTicket = false;

  // If set, requests local pseudonyms for the access group of the client.
  bool includeAccessGroupPseudonyms = false;
};

// Used as arguments for CoreClient::enumerateAndRetrieveData2
struct enumerateAndRetrieveData2Opts{
  std::vector<std::string> groups;
  std::vector<PolymorphicPseudonym> pps;
  std::vector<std::string> columnGroups;
  std::vector<std::string> columns;

  // Whether to include data in response.  Other conditions, like
  // dataSizeLimit, might prevent data to be included.
  bool includeData = true;

  // Limit on the size of the data to include with the response.
  uint64_t dataSizeLimit = 0;

  // If set, try to use this ticket (if it matches the query).
  // Otherwise, request a new ticket (unless forceTicket is set).
  // Warning: tickets have timestamps.  Reusing an old ticket will yield old
  // data.
  std::shared_ptr<IndexedTicket2> ticket = nullptr;

  // If set, forces the usage of the provided ticket.
  bool forceTicket = false;

  // If set, requests local pseudonyms for the access group of the client.
  bool includeAccessGroupPseudonyms = false;
};

// Used as an argument for CoreClient::storeData2
struct storeData2Opts {
  // If set, try to use this ticket (if it matches the storage request).
  // Warning: tickets have timestamps.  Reusing an old ticket will yield old
  // data.
  std::shared_ptr<IndexedTicket2> ticket = nullptr;

  // If set, forces the usage of the provided ticket.
  bool forceTicket = false;
};

class ShortPseudonymFormatError : public std::runtime_error {
 public:
  ShortPseudonymFormatError(const std::string& sp) noexcept
      : std::runtime_error("Short pseudonym '" + sp + "' does not look like a short pseudonym")
  {}
};

class ShortPseudonymContextError : public std::runtime_error {
 public:
  ShortPseudonymContextError(const std::string& sp, const std::string& context) noexcept
    : std::runtime_error("Short pseudonym '" + sp + "' is not available in the current (" + context + ") context")
  {}
};

class CoreClient : boost::noncopyable {
 public:
  static constexpr size_t DATA_RETRIEVAL_BATCH_SIZE{4000};

 private:
  std::shared_ptr<boost::asio::io_context> io_context;
  std::optional<std::filesystem::path> keysFilePath;
  const std::filesystem::path caCertFilepath;
  AsymmetricKey privateKey;
  std::shared_ptr<WorkerPool> mWorkerPool = nullptr;

  std::shared_ptr<WorkerPool> getWorkerPool();

  X509RootCertificates rootCAs;
  X509CertificateChain certificateChain;

  ElgamalPrivateKey privateKeyData;
  const ElgamalPublicKey publicKeyData;
  ElgamalPrivateKey privateKeyPseudonyms;
  const ElgamalPublicKey publicKeyPseudonyms;
  std::shared_ptr<GlobalConfiguration> mGlobalConf;

  const EndPoint accessManagerEndPoint;
  const EndPoint storageFacilityEndPoint;
  const EndPoint transcryptorEndPoint;

  std::shared_ptr<messaging::ServerConnection> clientAccessManager;
  std::shared_ptr<messaging::ServerConnection> clientStorageFacility;
  std::shared_ptr<messaging::ServerConnection> clientTranscryptor;

  rxcpp::subjects::subject<int> registrationSubject;
  rxcpp::subjects::subject<EnrollmentResult> enrollmentSubject;

 public:
  class Builder {
   public:
    Builder& setIoContext(std::shared_ptr<boost::asio::io_context> io_context) {
      this->io_context = io_context;
      return *this;
    }
    std::shared_ptr<boost::asio::io_context> getIoContext() const {
      return io_context;
    }

    Builder& setKeysFilePath(const std::filesystem::path& keysFilePath) {
      this->keysFilePath = keysFilePath;
      return *this;
    }
    const std::optional<std::filesystem::path>& getKeysFilePath() const {
      return keysFilePath;
    }

    Builder& setCaCertFilepath(const std::filesystem::path& caCertFilepath) {
      this->caCertFilepath = std::filesystem::canonical(caCertFilepath);
      return *this;
    }
    const std::filesystem::path& getCaCertFilepath() const {
      return caCertFilepath;
    }

    const AsymmetricKey& getPrivateKey() const {
      return privateKey;
    }
    Builder& setPrivateKey(const AsymmetricKey& privateKey) {
      Builder::privateKey = privateKey;
      return *this;
    }

    const X509CertificateChain& getCertificateChain() const {
      return certificateChain;
    }
    Builder& setCertificateChain(const X509CertificateChain& certificateChain) {
      Builder::certificateChain = certificateChain;
      return *this;
    }

    const ElgamalPrivateKey& getPrivateKeyData() const {
      return privateKeyData;
    }
    Builder& setPrivateKeyData(const ElgamalPrivateKey& privateKeyData) {
      Builder::privateKeyData = privateKeyData;
      return *this;
    }

    const ElgamalPrivateKey& getPrivateKeyPseudonyms() const {
      return privateKeyPseudonyms;
    }
    Builder& setPrivateKeyPseudonyms(const ElgamalPrivateKey& privateKeyPseudonyms) {
      Builder::privateKeyPseudonyms = privateKeyPseudonyms;
      return *this;
    }

    const ElgamalPublicKey& getPublicKeyData() const {
      return publicKeyData;
    }
    Builder& setPublicKeyData(const ElgamalPublicKey& publicKeyData) {
      Builder::publicKeyData = publicKeyData;
      return *this;
    }

    const ElgamalPublicKey& getPublicKeyPseudonyms() const {
      return publicKeyPseudonyms;
    }
    Builder& setPublicKeyPseudonyms(const ElgamalPublicKey& publicKeyPseudonyms) {
      Builder::publicKeyPseudonyms = publicKeyPseudonyms;
      return *this;
    }

    const EndPoint& getAccessManagerEndPoint() const {
      return accessManagerEndPoint;
    }
    Builder& setAccessManagerEndPoint(const EndPoint& accessManagerEndPoint) {
      Builder::accessManagerEndPoint = accessManagerEndPoint;
      return *this;
    }

    const EndPoint& getStorageFacilityEndPoint() const {
      return storageFacilityEndPoint;
    }
    Builder& setStorageFacilityEndPoint(const EndPoint& storageFacilityEndPoint) {
      Builder::storageFacilityEndPoint = storageFacilityEndPoint;
      return *this;
    }

    const EndPoint& getTranscryptorEndPoint() const {
      return transcryptorEndPoint;
    }
    Builder& setTranscryptorEndPoint(const EndPoint& transcryptorEndPoint) {
      Builder::transcryptorEndPoint = transcryptorEndPoint;
      return *this;
    }

    void initialize(const Configuration& config,
                    std::shared_ptr<boost::asio::io_context> io_context,
                    bool persistKeysFile);

    std::shared_ptr<CoreClient> build() const {
      return std::shared_ptr<CoreClient>(new CoreClient(*this));
    }

   private:
    std::shared_ptr<boost::asio::io_context> io_context;
    std::optional<std::filesystem::path> keysFilePath;
    std::filesystem::path caCertFilepath;
    AsymmetricKey privateKey;
    X509CertificateChain certificateChain;
    ElgamalPrivateKey privateKeyData;
    ElgamalPrivateKey privateKeyPseudonyms;
    ElgamalPublicKey publicKeyData;
    ElgamalPublicKey publicKeyPseudonyms;
    EndPoint accessManagerEndPoint;
    EndPoint storageFacilityEndPoint;
    EndPoint transcryptorEndPoint;
  };

  /*!
   * \brief Generate a polymorphic pseudonym for a registered participant.
   */
  PolymorphicPseudonym generateParticipantPolymorphicPseudonym(const std::string& participantSID);
  const ElgamalPublicKey& getPublicKeyPseudonyms() const {
    return publicKeyPseudonyms;
  }

  /*!
   * \brief Interpret each string as a textually represented polymorphic pseudonym, or a participant identifier,
   * or a local pseudonym, or a participant alias (aka user pseudonym, i.e. a shortened local pseudonym).
   * Convert to polymorphic pseudonym in all cases.
   * \param idsAndOrPps (User-provided) participant specification
   * \return ((An observable emitting) a shared_ptr to) a vector of PolymorphicPseudonym corresponding with the vector of input strings.
   */
  rxcpp::observable<std::shared_ptr<std::vector<PolymorphicPseudonym>>> parsePpsOrIdentities(const std::vector<std::string>& idsAndOrPps);

  /*!
   * \brief Interpret a string as a textually represented polymorphic pseudonym, or a participant identifier,
   * or a local pseudonym, or a participant alias (aka user pseudonym, i.e. a shortened local pseudonym).
   * Convert to a polymorphic pseudonym in all cases.
   */
  rxcpp::observable<PolymorphicPseudonym> parsePPorIdentity(const std::string& participantIdOrPP);

  /*!
   * \brief Check whether the client is enrolled.
   */
  bool getEnrolled();


  // Returns the access group as which the client is enrolled.
  std::string getEnrolledGroup();

  // Returns the name of the user for which the client is enrolled.
  std::string getEnrolledUser() const;

  /*!
   * \brief Enroll a non-user facility. The type of facility is inferred from this CoreClient's certificate chain.
   *
   * \return rxcpp::observable< EnrollmentResult >
   */
  rxcpp::observable<EnrollmentResult> enrollServer();

  /*!
   * \brief Store data in PEP using the new API
   */
  rxcpp::observable<DataStorageResult2> storeData2(
    const PolymorphicPseudonym& pp,
    const std::string& column,
    std::shared_ptr<std::string> data,
    const std::vector<NamedMetadataXEntry>& xentries = {},
    const storeData2Opts& opts={});
  rxcpp::observable<DataStorageResult2> storeData2(
    const std::vector<StoreData2Entry>& entries,
    const storeData2Opts& opts={});

  rxcpp::observable<DataStorageResult2> updateMetadata2(
    const std::vector<StoreMetadata2Entry>& entries,
    const storeData2Opts& opts = {});

  rxcpp::observable<HistoryResult> deleteData2(
    const PolymorphicPseudonym& pp,
    const std::string& column,
    const storeData2Opts& opts = {});
  rxcpp::observable<HistoryResult> deleteData2(
    const std::vector<Storage2Entry>& entries,
    const storeData2Opts& opts = {});

  /*!
   * \brief Enuremate and retrieve using new API.
   *
   * This function loads the full contents of the files into memory and
   * should thus only be used for small files.
   *
   * If dataSizeLimit is non-zero, then only data of files will be retrieved
   * that are smaller than the specified limit.
   */
  rxcpp::observable<EnumerateAndRetrieveResult>
  enumerateAndRetrieveData2(const enumerateAndRetrieveData2Opts& opts);

  /*!
   * \brief Requests (or reuses) a new-style Ticket.
   *
   * If opts.ticket is set, the function will check whether that ticket
   * has at least as much scope as the requested ticket.  If it does,
   * then it will simply return opts.ticket.  Otherwise, it will request
   * a new ticket.
   */
  rxcpp::observable<IndexedTicket2>
  requestTicket2(const requestTicket2Opts& opts);

  /*!
   * \brief Enumerate data using a pre-requested ticket.
   */
  rxcpp::observable<std::vector<EnumerateResult>>
  enumerateData2(std::shared_ptr<SignedTicket2> ticket);

  /*!
   * \brief Enuremate data using new API.
   * \remark Results won't include (local) pseudonyms for the access group.
   */
  rxcpp::observable<std::vector<EnumerateResult>>
  enumerateData2(
    const std::vector<std::string>& groups=std::vector<std::string>(),
    const std::vector<PolymorphicPseudonym>& pps = {},
    const std::vector<std::string>& columnGroups=std::vector<std::string>(),
    const std::vector<std::string>& columns=std::vector<std::string>());

  rxcpp::observable<EnumerateResult>
  getMetadata(const std::vector<std::string>& ids, std::shared_ptr<SignedTicket2> ticket);

  rxcpp::observable<std::shared_ptr<RetrieveResult>>
  retrieveData2(
    const rxcpp::observable<EnumerateResult>& subjects,
    std::shared_ptr<SignedTicket2> ticket,
    bool includeContent);

  /*!
 * \brief Retrieve history using a pre-requested ticket.
 */
  rxcpp::observable<std::vector<HistoryResult>>
    getHistory2(SignedTicket2 ticket,
      const std::optional<std::vector<PolymorphicPseudonym>>& pps = std::nullopt,
      const std::optional<std::vector<std::string>>& columns = std::nullopt);

  rxcpp::observable<std::shared_ptr<GlobalConfiguration>> getGlobalConfiguration();
  rxcpp::observable<VerifiersResponse> getRSKVerifiers();

  rxcpp::observable<std::shared_ptr<ColumnNameMappings>> getColumnNameMappings();
  rxcpp::observable<std::shared_ptr<ColumnNameMappings>> readColumnNameMapping(const ColumnNameSection& original);
  rxcpp::observable<std::shared_ptr<ColumnNameMappings>> createColumnNameMapping(const ColumnNameMapping& mapping);
  rxcpp::observable<std::shared_ptr<ColumnNameMappings>> updateColumnNameMapping(const ColumnNameMapping& mapping);
  rxcpp::observable<FakeVoid> deleteColumnNameMapping(const ColumnNameSection& original);

  // Get/set non-cell metadata
  rxcpp::observable<std::shared_ptr<StructureMetadataEntry>> getStructureMetadata(
      StructureMetadataType subjectType,
      std::vector<std::string> subjects,
      std::vector<StructureMetadataKey> keys = {});
  rxcpp::observable<FakeVoid> setStructureMetadata(StructureMetadataType subjectType, rxcpp::observable<std::shared_ptr<StructureMetadataEntry>> entries);
  rxcpp::observable<FakeVoid> removeStructureMetadata(StructureMetadataType subjectType, std::vector<StructureMetadataSubjectKey> subjectKeys);

  static constexpr bool DEFAULT_PERSIST_KEYS_FILE = true;

  static std::shared_ptr<CoreClient> OpenClient(
      const Configuration& config,
      std::shared_ptr<boost::asio::io_context> io_context = nullptr,
      bool persistKeysFile = DEFAULT_PERSIST_KEYS_FILE);

protected:
  /*! \brief constructor for CoreClient
   *
   * \param builder A builder that builds a CoreClient
   */
  CoreClient(const Builder& builder);

  /// Returns a signed copy of \p msg, using the details of the current interactive user
  template <typename MessageP>
  auto sign(MessageP&& msg) {
    using SignedMessage = Signed<std::remove_cvref_t<MessageP>>;
    return SignedMessage{std::forward<MessageP>(msg), certificateChain, privateKey};
  }

  std::shared_ptr<messaging::ServerConnection> tryConnectTo(const EndPoint& endPoint);
  rxcpp::observable<VersionResponse> tryGetServerVersion(std::shared_ptr<messaging::ServerConnection> connection) const;
  rxcpp::observable<SignedPingResponse> pingSigningServer(std::shared_ptr<messaging::ServerConnection> connection) const;

  struct EnrollmentContext {
    std::shared_ptr<AsymmetricKey> privateKey;
    X509CertificateChain certificateChain;
    CurveScalar alpha, beta;
    CurveScalar gamma, delta;
    SignedKeyComponentRequest keyComponentRequest;
  };

  rxcpp::observable<EnrollmentResult> completeEnrollment(std::shared_ptr<EnrollmentContext> context);

public:
  virtual ~CoreClient() noexcept = default;

  rxcpp::observable<ColumnAccess> getAccessibleColumns(bool includeImplicitlyGranted, const std::vector<std::string>& requireModes = {});
  rxcpp::observable<std::string> getInaccessibleColumns(const std::string& mode, rxcpp::observable<std::string> columns);
  rxcpp::observable<ParticipantGroupAccess> getAccessibleParticipantGroups(bool includeImplicitlyGranted);

  rxcpp::observable<int> getRegistrationExpiryObservable();
  inline const std::optional<std::filesystem::path>& getKeysFilePath() const noexcept { return keysFilePath; }

  rxcpp::observable<ConnectionStatus> getAccessManagerConnectionStatus();
  rxcpp::observable<ConnectionStatus> getStorageFacilityStatus();
  rxcpp::observable<ConnectionStatus> getTranscryptorStatus();

  rxcpp::observable<VersionResponse> getAccessManagerVersion();
  rxcpp::observable<VersionResponse> getTranscryptorVersion();
  rxcpp::observable<VersionResponse> getStorageFacilityVersion();

  rxcpp::observable<SignedPingResponse> pingAccessManager() const;
  rxcpp::observable<SignedPingResponse> pingTranscryptor() const;
  rxcpp::observable<SignedPingResponse> pingStorageFacility() const;

  rxcpp::observable<MetricsResponse> getAccessManagerMetrics();
  rxcpp::observable<MetricsResponse> getTranscryptorMetrics();
  rxcpp::observable<MetricsResponse> getStorageFacilityMetrics();

  rxcpp::observable<std::shared_ptr<std::vector<std::optional<PolymorphicPseudonym>>>> findPpsForShortPseudonyms(const std::vector<std::string>& sps, const std::optional<StudyContext>& studyContext = std::nullopt);
  rxcpp::observable<PolymorphicPseudonym> findPPforShortPseudonym(std::string shortPseudonym, const std::optional<StudyContext>& studyContext = std::nullopt);

  rxcpp::observable<LocalPseudonyms> getLocalizedPseudonyms();

  const std::shared_ptr<boost::asio::io_context>& getIoContext() const;
  virtual rxcpp::observable<FakeVoid> shutdown();

  //
  // Access manager administration API
  //
  rxcpp::observable<FakeVoid>
  amaCreateColumn(std::string name);

  rxcpp::observable<FakeVoid>
  amaRemoveColumn(std::string name);

  rxcpp::observable<FakeVoid>
  amaCreateColumnGroup(std::string name);

  /*!
   * Removes the column group named \a name
   * @param force determines how associated columns and access rules
   *        are handled\n
   *        \c false - the removal of the group is aborted\n
   *        \c true - the associated data is also removed
   */
  rxcpp::observable<FakeVoid>
  amaRemoveColumnGroup(std::string name, bool force);

  rxcpp::observable<FakeVoid>
  amaAddColumnToGroup(std::string column, std::string group);

  rxcpp::observable<FakeVoid>
  amaRemoveColumnFromGroup(std::string column, std::string group);

  rxcpp::observable<FakeVoid>
  amaCreateParticipantGroup(std::string name);

  /*!
   * Removes the participant group named \a name
   * @param force determines how associated participant connections and access rules
   *        are handled\n
   *        \c false - the removal of the group is aborted\n
   *        \c true - the associated data is also removed
   */
  rxcpp::observable<FakeVoid>
  amaRemoveParticipantGroup(std::string name, bool force);

  rxcpp::observable<FakeVoid>
  amaAddParticipantToGroup(std::string group, const PolymorphicPseudonym& participant);

  rxcpp::observable<FakeVoid>
  amaRemoveParticipantFromGroup(std::string group, const PolymorphicPseudonym& participant);

  rxcpp::observable<FakeVoid>
  amaCreateColumnGroupAccessRule(std::string columnGroup,
      std::string accessGroup, std::string mode);

  rxcpp::observable<FakeVoid>
  amaRemoveColumnGroupAccessRule(std::string columnGroup,
      std::string accessGroup, std::string mode);

  rxcpp::observable<FakeVoid>
  amaCreateGroupAccessRule(std::string group,
      std::string accessGroup, std::string mode);

  rxcpp::observable<FakeVoid>
  amaRemoveGroupAccessRule(std::string group,
      std::string accessGroup, std::string mode);

  rxcpp::observable<AmaQueryResponse>
  amaQuery(AmaQuery query);

  //
  // User administration API
  //
  rxcpp::observable<FakeVoid> createUser(std::string uid);

  rxcpp::observable<FakeVoid> removeUser(std::string uid);

  rxcpp::observable<FakeVoid> addUserIdentifier(std::string existingUid, std::string newUid);

  rxcpp::observable<FakeVoid> removeUserIdentifier(std::string uid);

  rxcpp::observable<FakeVoid> createUserGroup(UserGroup userGroup);

  rxcpp::observable<FakeVoid> modifyUserGroup(UserGroup userGroup);

  rxcpp::observable<FakeVoid> removeUserGroup(std::string name);

  rxcpp::observable<FakeVoid> addUserToGroup(std::string uid, std::string group);

  rxcpp::observable<FakeVoid> removeUserFromGroup(std::string uid, std::string group, bool blockTokens);

  rxcpp::observable<UserQueryResponse> userQuery(UserQuery query);

  // Private helpers
 private:
  struct AESKey;

  rxcpp::observable<FakeVoid> amaRequestMutation(AmaMutationRequest request);
  // Unblinds and decrypt keys for entries.  The provided ticket should be
  // the same as used to retrieve the entries.
  rxcpp::observable<std::vector<AESKey>>
  unblindAndDecryptKeys(
      const std::vector<EnumerateResult>& entries,
      std::shared_ptr<SignedTicket2> ticket);

  // Assigns encrypted and blinded keys to (entries in) a data storage request.
  rxcpp::observable<FakeVoid> encryptAndBlindKeys(
    std::shared_ptr<DataEntriesRequest2<DataStoreEntry2>> request,
    const std::vector<AESKey>& keys);

  class TicketPseudonyms;

  std::vector<EnumerateResult> convertDataEnumerationEntries(
    const std::vector<DataEnumerationEntry2>& entries,
    const TicketPseudonyms& pseudonyms) const;

  rxcpp::observable<FakeVoid> requestUserMutation(UserMutationRequest request);

};

struct CoreClient::AESKey {
    CurvePoint point;
    std::string bytes;

    explicit AESKey(const CurvePoint& point);
    AESKey() : AESKey(CurvePoint::Random()) {}
};

class CoreClient::TicketPseudonyms {
private:
  std::vector<std::shared_ptr<LocalPseudonyms>> mPseudonyms;
  std::optional<std::vector<std::shared_ptr<LocalPseudonym>>> mAgPseuds;

public:
  TicketPseudonyms(const SignedTicket2& ticket, const ElgamalPrivateKey& privateKeyPseudonyms);

  TicketPseudonyms(const TicketPseudonyms&) = delete;
  TicketPseudonyms& operator =(const TicketPseudonyms&) = delete;

  std::shared_ptr<LocalPseudonyms> getLocalPseudonyms(uint32_t index) const { return mPseudonyms.at(index); }
  std::shared_ptr<LocalPseudonym> getAccessGroupPseudonym(uint32_t index) const; // Returns NULL if ticket didn't include access group pseudonyms
};

}
