// Storage class for the transcryptor

#include <pep/transcryptor/Storage.hpp>

#include <pep/rsk/Proofs.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/database/Storage.hpp>

#include <sqlite_orm/sqlite_orm.h>

#include <boost/algorithm/hex.hpp>
#include <boost/range/algorithm/set_algorithm.hpp>

#include <cctype>
#include <numeric>
#include <utility>

// The schema of the database is defined in the ts_create_db() function.
// For each table, we have a struct <TableName>Record that represents a
// row in that table.


// CHANGES:
//
//
//     Version 1 -> Version 2:
//
// As the size of the version 1 database was dominated by certificates,
// see issue #1173, we moved the certificate chain from
// TicketRequestRecord::request's log_signature to its own table,
// CertificateChainRecord, and added TicketRequestRecord::certificateChain
// to refer to it.
//
// To be able to keep the checksum of existing records unchanged during the
// migration of a version 1 database to version 2, we added
// TicketRequestRecord::checksumCorrection to compensate, see the checksum
// method of TicketRequest.
//
// To detect and record whether the migration has been performed,
// we added the MigrationRecord table, see the ensureInitialized function.

using namespace std::chrono;
using namespace std::literals;

namespace pep {

using namespace sqlite_orm;

namespace {

const std::string LOG_TAG("TranscryptorStorage");

// Records past migrations performed to this database. (Since version 2.)
// When the database is initialised from stratch, this is recorded as
// a migration too (except in version 1), see "getCurrentVersion"
// and "migrate" functions for more details.
struct MigrationRecord {
  constexpr static uint64_t TargetVersion = 2;

  MigrationRecord() = default;
  MigrationRecord(uint64_t toVersion) {

    RandomBytes(this->checksumNonce, 16);
    this->toVersion = toVersion;
    this->timestamp = TicksSinceEpoch<milliseconds>(TimeNow());
  }

  uint64_t checksum() const {
    std::ostringstream os;
    os << this->seqno << '\0'
       << this->timestamp << '\0'
       << this->toVersion << '\0'
       << std::string(checksumNonce.begin(), checksumNonce.end());
    return UnpackUint64BE(Sha256().digest(std::move(os).str()));
  }

  int64_t seqno{};
  uint64_t toVersion{};
  UnixMillis timestamp{};
  std::vector<char> checksumNonce;
};

// Records a TicketRequest
struct TicketRequestRecord {
  TicketRequestRecord() = default;
  TicketRequestRecord(
      SignedTicketRequest2 ticketRequest,
      int64_t pseudonymSet,
      int64_t modeSet,
      std::string pseudonymHash,
      std::string accessGroup,
      std::optional<int64_t> certificateChain) : accessGroup(std::move(accessGroup)), pseudonymSet(pseudonymSet), modeSet(modeSet) {

    RandomBytes(this->checksumNonce, 16);

    // Work around https://github.com/fnc12/sqlite_orm/issues/245 (again).
    // See #826
    std::string idBytes;
    RandomBytes(idBytes, 16);
    this->id = boost::algorithm::hex(idBytes);

    this->pseudonymHash = std::vector<char>(
        pseudonymHash.begin(), pseudonymHash.end());
    this->request = Serialization::ToCharVector(ticketRequest);
    this->timestamp = TicksSinceEpoch<milliseconds>(TimeNow());
    this->certificateChain = certificateChain;
  }

  uint64_t checksum_v1() const {
    std::ostringstream os;
    os << seqno << '\0' << timestamp << '\0' << request.size() << '\0'
       << pseudonymSet << '\0' << modeSet << '\0'
       << std::string(request.begin(), request.end())
       << accessGroup.size() << '\0'
       << std::string(accessGroup.begin(), accessGroup.end())
       << std::string(id.begin(), id.end())
       << std::string(checksumNonce.begin(), checksumNonce.end())
       << std::string(pseudonymHash.begin(), pseudonymHash.end());
    return UnpackUint64BE(Sha256().digest(std::move(os).str()));
  }

  uint64_t checksum() const {
    std::ostringstream os;
    os << seqno << '\0' << timestamp << '\0' << request.size() << '\0'
       << pseudonymSet << '\0' << modeSet << '\0'
       << std::string(request.begin(), request.end())
       << accessGroup.size() << '\0'
       << std::string(accessGroup.begin(), accessGroup.end())
       << std::string(id.begin(), id.end())
       << std::string(checksumNonce.begin(), checksumNonce.end())
       << std::string(pseudonymHash.begin(), pseudonymHash.end()) << '\0'
       << (certificateChain ? *certificateChain + 1 : 0) << '\0';

    uint64_t correction = 0;
    if (this->checksumCorrection)
      correction = *(this->checksumCorrection);

    return UnpackUint64BE(Sha256().digest(std::move(os).str()))
             ^ correction;
  }

  std::string getPseudonymHash() {
    return std::string(pseudonymHash.begin(), pseudonymHash.end());
  }

  int64_t seqno{};
  std::vector<char> checksumNonce;

  // since version 2, optional because otherwise sync_schema drops the table,
  // since uint64_t will get the "NOT NULL" modifier.
  std::optional<uint64_t> checksumCorrection;

  std::string id;
  std::string accessGroup;
  UnixMillis timestamp{};

  std::vector<char> request; // SignedTicketRequest
  int64_t pseudonymSet{};
  int64_t modeSet{};

  // Hash of the encrypted local pseudonyms, so that the logger can verify
  // that the pseudonyms listed in the ticket are correct.
  std::vector<char> pseudonymHash;

  // For efficiency's sake, the certificate chain is stripped from
  // this->request's log_signature, and stored in a separate table.
  std::optional<int64_t> certificateChain; // since version 2
};

// Records a certificate chain (Since version 2.)
struct CertificateChainRecord {
  CertificateChainRecord() = default;
  CertificateChainRecord(
      std::vector<char>&& leaf,
      std::optional<int64_t> parent,
      std::vector<char>&& fingerprint)
   : parent(parent),
     leaf(std::move(leaf)),
     fingerprint(std::move(fingerprint))
  {
    RandomBytes(this->checksumNonce, 16);
  }

  uint64_t checksum() const {
    std::ostringstream os;
    os << this->seqno << '\0'
       << ( this->parent ? *(this->parent)+1 : 0 ) << '\0'
       << std::string(this->checksumNonce.begin(), this->checksumNonce.end())
       << std::string(this->leaf.begin(), this->leaf.end())
       << std::string(this->fingerprint.begin(), this->fingerprint.end());
    return UnpackUint64BE(Sha256().digest(std::move(os).str()));
  }

  int64_t seqno{};
  std::vector<char> checksumNonce;
  std::optional<int64_t> parent;
  std::vector<char> leaf;

  // fingerprint = sha256(leaf) + parent.fingerprint
  std::vector<char> fingerprint;
};

// Records an issued ticket
struct TicketIssueRecord {
  TicketIssueRecord() = default;
  TicketIssueRecord(int64_t request, int64_t columnSet, Timestamp ts)
  : timestamp(TicksSinceEpoch<milliseconds>(ts)), request(request), columnSet(columnSet) {
    RandomBytes(this->checksumNonce, 16);
  }

  uint64_t checksum() const {
    std::ostringstream os;
    os << seqno << timestamp << request << columnSet
       << std::string(checksumNonce.begin(), checksumNonce.end());
    return UnpackUint64BE(Sha256().digest(std::move(os).str()));
  }

  int64_t seqno{};
  std::vector<char> checksumNonce;
  UnixMillis timestamp{};

  int64_t request{}; // seqno of related TicketRequestRecord
  int64_t columnSet{}; // seqno of ColumnSetRecord granted access to
};

// Records an immutable set of local logger pseudonyms.
struct PseudonymSetRecord {
  PseudonymSetRecord() = default;
  PseudonymSetRecord(std::string key) : key(std::move(key)) {
    RandomBytes(this->checksumNonce, 16);
  }

  uint64_t checksum() const {
    std::ostringstream os;
    os << seqno << std::string(key.begin(), key.end())
       << std::string(checksumNonce.begin(), checksumNonce.end());
    return UnpackUint64BE(Sha256().digest(std::move(os).str()));
  }

  int64_t seqno{};
  std::vector<char> checksumNonce;

  // To look up whether a record already exists for a set of pseudonyms,
  // a key is derived from the pseudonyms, by lexicographically sorting
  // their packed representation and then computing a hash of the
  // concatenation.
  std::string key;
};

// Records which pseudonym to which pseudonym record
struct PseudonymSetPseudonymRecord {
  PseudonymSetPseudonymRecord() = default;
  PseudonymSetPseudonymRecord(const LocalPseudonym& pseudonym, int64_t set) : set(set) {
    this->pseudonym = Serialization::ToCharVector(pseudonym.getValidCurvePoint());
    RandomBytes(this->checksumNonce, 16);
  }

  uint64_t checksum() const {
    std::ostringstream os;
    os << seqno << set
       << std::string(checksumNonce.begin(), checksumNonce.end())
       << std::string(pseudonym.begin(), pseudonym.end());
    return UnpackUint64BE(Sha256().digest(std::move(os).str()));
  }

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t set{};
  std::vector<char> pseudonym;
};

// Records an immutable set of local logger pseudonyms.
struct ColumnSetRecord {
  ColumnSetRecord() = default;
  ColumnSetRecord(std::string key) : key(std::move(key)) {
    RandomBytes(this->checksumNonce, 16);
  }

  uint64_t checksum() const {
    std::ostringstream os;
    os << seqno << std::string(key.begin(), key.end())
       << std::string(checksumNonce.begin(), checksumNonce.end());
    return UnpackUint64BE(Sha256().digest(std::move(os).str()));
  }

  int64_t seqno{};
  std::vector<char> checksumNonce;

  // To look up whether a record already exists for a set of columns,
  // a key is derived from the columns, by lexicographically sorting
  // themnd then computing a hash of the concatenation.
  std::string key;
};

// Records which column belongs to which column set
struct ColumnSetColumnRecord {
  ColumnSetColumnRecord() = default;
  ColumnSetColumnRecord(const std::string& column, int64_t set) : set(set), column(column) {
    RandomBytes(this->checksumNonce, 16);
  }

  uint64_t checksum() const {
    std::ostringstream os;
    os << seqno << column << set << column.size() << column
       << std::string(checksumNonce.begin(), checksumNonce.end());
    return UnpackUint64BE(Sha256().digest(std::move(os).str()));
  }

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t set{};
  std::string column;
};

// Records an immutable set of modes.
struct ModeSetRecord {
  ModeSetRecord() = default;
  ModeSetRecord(std::string key) : key(std::move(key)) {
    RandomBytes(this->checksumNonce, 16);
  }

  uint64_t checksum() const {
    std::ostringstream os;
    os << seqno << std::string(key.begin(), key.end())
       << std::string(checksumNonce.begin(), checksumNonce.end());
    return UnpackUint64BE(Sha256().digest(std::move(os).str()));
  }

  int64_t seqno{};
  std::vector<char> checksumNonce;

  // To look up whether a record already exists for a set of modes,
  // a key is derived from the modes, by lexicographically sorting
  // them and then computing a hash of the concatenation.
  std::string key;
};

// Records which mode belongs to which mode set
struct ModeSetModeRecord {
  ModeSetModeRecord() = default;
  ModeSetModeRecord(const std::string& mode, int64_t set) : set(set), mode(mode) {
    RandomBytes(this->checksumNonce, 16);
  }

  uint64_t checksum() const {
    std::ostringstream os;
    os << seqno << mode << set << mode.size() << mode
       << std::string(checksumNonce.begin(), checksumNonce.end());
    return UnpackUint64BE(Sha256().digest(std::move(os).str()));
  }

  int64_t seqno{};
  std::vector<char> checksumNonce;
  int64_t set{};
  std::string mode;
};

// This function defines the database scheme used.
auto ts_create_db(const std::string& path) {
  // BEWARE!  Changing a column below causes the whole table to be dropped!
  // by the call to "sync_storage" below.  Adding and removing columns,
  // on the other hand, should be fine.  For more info, see:
  //
  //   https://github.com/fnc12/sqlite_orm#migrations-functionality
  //   https://github.com/fnc12/sqlite_orm/wiki/storage_t%3A%3Async_schema
  //
  // See also the "ensureIntitialized" function below.
  return make_storage(path,
    make_table("Migration",
      make_column("timestamp", &MigrationRecord::timestamp),
      make_column("toVersion", &MigrationRecord::toVersion),
      make_column("checksumNonce", &MigrationRecord::checksumNonce),
      make_column("seqno",
        &MigrationRecord::seqno,
        primary_key().autoincrement())),

    make_index("idx_TicketRequest_id",
      &TicketRequestRecord::id),
    make_table("TicketRequest",
      make_column("request", &TicketRequestRecord::request),
      make_column("pseudonymSet", &TicketRequestRecord::pseudonymSet),
      make_column("modeSet", &TicketRequestRecord::modeSet),
      make_column("pseudonymHash", &TicketRequestRecord::pseudonymHash),
      make_column("accessGroup", &TicketRequestRecord::accessGroup),
      make_column("timestamp", &TicketRequestRecord::timestamp),
      make_column("checksumNonce", &TicketRequestRecord::checksumNonce),
      make_column("checksumCorrection",
        &TicketRequestRecord::checksumCorrection),
      make_column("certificateChain", &TicketRequestRecord::certificateChain),
      make_column("id", &TicketRequestRecord::id),
      make_column("seqno",
        &TicketRequestRecord::seqno,
        primary_key().autoincrement())),

    make_index("idx_CertificateChain_fingerprint",
      &CertificateChainRecord::fingerprint),
    make_table("CertificateChain",
      make_column("parent", &CertificateChainRecord::parent),
      make_column("leaf", &CertificateChainRecord::leaf),
      make_column("checksumNonce", &CertificateChainRecord::checksumNonce),
      make_column("fingerprint", &CertificateChainRecord::fingerprint),
      make_column("seqno",
        &CertificateChainRecord::seqno,
        primary_key().autoincrement())),

    make_table("TicketIssue",
      make_column("request", &TicketIssueRecord::request),
      make_column("columnSet", &TicketIssueRecord::columnSet),
      make_column("timestamp", &TicketIssueRecord::timestamp),
      make_column("checksumNonce", &TicketIssueRecord::checksumNonce),
      make_column("seqno",
        &TicketIssueRecord::seqno,
        primary_key().autoincrement())),

    make_index("idx_PseudonymSet_key",
      &PseudonymSetRecord::key),
    make_table("PseudonymSet",
      make_column("checksumNonce", &PseudonymSetRecord::checksumNonce),
      make_column("key", &PseudonymSetRecord::key),
      make_column("seqno",
        &PseudonymSetRecord::seqno,
        primary_key().autoincrement())),

    make_index("idx_PseudonymSetPseudonym_pseudonym",
      &PseudonymSetPseudonymRecord::pseudonym),
    make_index("idx_PseudonymSetPseudonym_set",
      &PseudonymSetPseudonymRecord::set),
    make_table("PseudonymSetPseudonym",
      make_column("pseudonym", &PseudonymSetPseudonymRecord::pseudonym),
      make_column("set", &PseudonymSetPseudonymRecord::set),
      make_column("checksumNonce", &PseudonymSetPseudonymRecord::checksumNonce),
      make_column("seqno",
        &PseudonymSetPseudonymRecord::seqno,
        primary_key().autoincrement())),

    make_index("idx_ColumnSet_key",
      &ColumnSetRecord::key),
    make_table("ColumnSet",
      make_column("checksumNonce", &ColumnSetRecord::checksumNonce),
      make_column("key", &ColumnSetRecord::key),
      make_column("seqno",
        &ColumnSetRecord::seqno,
        primary_key().autoincrement())),

    make_index("idx_ColumnSetColumn_column",
      &ColumnSetColumnRecord::column),
    make_index("idx_ColumnSetColumn_set",
      &ColumnSetColumnRecord::set),
    make_table("ColumnSetColumn",
      make_column("column", &ColumnSetColumnRecord::column),
      make_column("set", &ColumnSetColumnRecord::set),
      make_column("checksumNonce", &ColumnSetColumnRecord::checksumNonce),
      make_column("seqno",
        &ColumnSetColumnRecord::seqno,
        primary_key().autoincrement())),

    make_index("idx_ModeSet_key",
      &ModeSetRecord::key),
    make_table("ModeSet",
      make_column("checksumNonce", &ModeSetRecord::checksumNonce),
      make_column("key", &ModeSetRecord::key),
      make_column("seqno",
        &ModeSetRecord::seqno,
        primary_key().autoincrement())),

    make_index("idx_ModeSetMode_column",
      &ModeSetModeRecord::mode),
    make_index("idx_ModeSetMode_set",
      &ModeSetModeRecord::set),
    make_table("ModeSetMode",
      make_column("mode", &ModeSetModeRecord::mode),
      make_column("set", &ModeSetModeRecord::set),
      make_column("checksumNonce", &ModeSetModeRecord::checksumNonce),
      make_column("seqno",
        &ModeSetModeRecord::seqno,
        primary_key().autoincrement()))
  );
}

}

// Can't be a typedef because we need to forward declare it for our pimpl idiom
struct TranscryptorStorageBackend : database::Storage<ts_create_db> {
  using Storage::Storage;
};

namespace {
template <typename TRecord>
class ChecksumChainFor : public transcryptor::ChecksumChain {
protected:
  Result calculate(std::shared_ptr<TranscryptorStorageBackend> storage, const Result& partial, uint64_t maxCheckpoint) const override {
    assert(partial.checkpoint < maxCheckpoint);
    auto result = partial;

    int64_t minSeqNo = -1; // The full chain includes all sequence numbers (0 or higher, i.e. greater than -1)
    if (partial.checkpoint != EMPTY_TABLE_CHECKPOINT) { // If we have a (previously calculated) partial result...
      assert(partial.checkpoint > EMPTY_TABLE_CHECKPOINT);
      minSeqNo = CheckpointToSeqNo(partial.checkpoint); // ... only process records (with sequence numbers) that aren't included in the partial result yet
    }

    for (const auto& record : storage->raw.iterate<TRecord>(
      where(c(&TRecord::seqno) > minSeqNo // Only process entries that aren't included in the "partial" result yet
        && c(&TRecord::seqno) <= CheckpointToSeqNo(maxCheckpoint)))) { // Only process entries up to (and including) the specified "maxCheckpoint"

      // Keep track of the highest checkpoint encountered: we're iterating/processing in arbitrary order
      result.checkpoint = std::max(result.checkpoint, SeqNoToCheckpoint(record.seqno));
      result.checksum ^= record.checksum();
        }

    return result;
  }

public:
  explicit ChecksumChainFor(std::string name) noexcept : ChecksumChain(std::move(name)) {}
};
}

TranscryptorStorage::TranscryptorStorage(
    const std::filesystem::path& path) : mPath(path.string()) {
  mStorage = std::make_shared<TranscryptorStorageBackend>(path.string());

  ensureInitialized();

  // Can't assign an { initializer-list } to mChecksumChains because it requires copy construction of the elements, which std::unique_ptr<> doesn't support
  mChecksumChains.insert(std::make_unique<ChecksumChainFor<MigrationRecord>>("migration"));
  mChecksumChains.insert(std::make_unique<ChecksumChainFor<TicketRequestRecord>>("ticket-request"));
  mChecksumChains.insert(std::make_unique<ChecksumChainFor<TicketIssueRecord>>("ticket-issue"));
  mChecksumChains.insert(std::make_unique<ChecksumChainFor<PseudonymSetRecord>>("pseudonym-set"));
  mChecksumChains.insert(std::make_unique<ChecksumChainFor<PseudonymSetPseudonymRecord>>("pseudonym-set-pseudonym"));
  mChecksumChains.insert(std::make_unique<ChecksumChainFor<ColumnSetRecord>>("column-set"));
  mChecksumChains.insert(std::make_unique<ChecksumChainFor<ColumnSetColumnRecord>>("column-set-column"));
  mChecksumChains.insert(std::make_unique<ChecksumChainFor<ModeSetRecord>>("mode-set"));
  mChecksumChains.insert(std::make_unique<ChecksumChainFor<ModeSetModeRecord>>("mode-set-mode"));
}

// Makes sure that the database is correctly initialized.  Adds columns
// and tables, and migrates, if necessary.  Rolls back on error.
void TranscryptorStorage::ensureInitialized() {
  bool migrated = false;

  try {
    auto guard = this->mStorage->raw.transaction_guard();
    this->ensureInitialized_unguarded(migrated);
    guard.commit();
  } catch (...) {
    LOG(LOG_TAG, error) << "Failed to initialize transcryptor storage! ";
    throw;
  }

  // After migration there might be a lot of unused pages,
  // so reclaim them using the VACUUM command.
  //
  // (Recall that the VACUUM command cannot be executed while any other
  //  transaction is active, so we do it here.)
  if (migrated) {
    LOG(LOG_TAG, info) << "Vacuuming database after migration.";
    this->mStorage->raw.vacuum();
    LOG(LOG_TAG, info) << "Vacuumed database.";
  }
}

void TranscryptorStorage::ensureInitialized_unguarded(bool& migrated) {
  if (!mStorage->syncSchema()) {
    LOG(LOG_TAG, info) << "All database schemas in sync;"
      << " checking whether migration has been performed.";

    std::optional<uint64_t> version = this->getCurrentVersion();

    if (!version) {
      LOG(LOG_TAG, error) << "Database schemas are in sync, "
        << "but no migration has been recorded!";
      throw Error("Detected potentially incomplete migration.");
    }

    if (*version < MigrationRecord::TargetVersion) {
      LOG(LOG_TAG, error) << "Database schemas are in sync, "
        << "but not all migrations have been performed!";
      throw Error("Detected potentially incomplete migration.");
    }

    // the folliwing should be ensured by this->getCurrentVersion()
    assert(version==MigrationRecord::TargetVersion);

    LOG(LOG_TAG, info) << "Database has already been migrated to "
      << "the current version, "
      << MigrationRecord::TargetVersion << ".";

    return;
  }
  // Not everyting was in sync, but no tables or columns were removed.
  // This happens in two cases:
  //   I.  when the database was empty;
  //   II. when a database of a different version was loaded.

  // To determine whether we're in case I we use the following heuristic:
  if(this->mStorage->raw.count<TicketRequestRecord>(limit(1)) == 0 &&
      this->mStorage->raw.count<MigrationRecord>(limit(1)) == 0) {

    LOG(LOG_TAG, warning) << "Detected empty database.";
    // Record current version:
    this->mStorage->raw.insert(MigrationRecord(MigrationRecord::TargetVersion));
    LOG(LOG_TAG, warning) << "Recorded migration to version "
        << MigrationRecord::TargetVersion;
    return;
  }

  LOG(LOG_TAG, warning) << "Migrating ...";

  migrate();
  migrated = true; // let the caller know he should vacuum
}

void TranscryptorStorage::migrate() {
  std::optional<int64_t> version = getCurrentVersion();

  if (version) {
    std::string message = "The need for a migration of the "
      "transcryptor database was detected, but we did not expect to find "
      "a record of a previous migration (to version "
      + std::to_string(*version) + ".)";
    LOG(LOG_TAG, error) << message;
    throw Error(message);
  }

  try {
    migrate_from_v1_to_v2();
  } catch (...) {
    LOG(LOG_TAG, error) << "Migration of transcryptor database from version 1"
       " to version 2 failed.";
    throw;
  }

  LOG(LOG_TAG, warning) << "Migrated successfully to version 2.";
}


void TranscryptorStorage::migrate_from_v1_to_v2() {
  int done = 0; // for progress logging

  LOG(LOG_TAG, warning) << "Migrating ticket requests ...";
  for (auto record : this->mStorage->raw.iterate<TicketRequestRecord>()) {
    // store old checksum before we modify record
    uint64_t old_checksum = record.checksum_v1();

    auto request = Serialization::FromCharVector<SignedTicketRequest2>(record.request);

    if (!request.mLogSignature) {
      LOG(LOG_TAG, warning) << "Ticket request record number "
        << record.seqno << " has no log signature!";
      // although troublesome, it does not affect migration, so we ...
      continue;
    }

    // move certificate chain from request to table:
    std::optional<int64_t> chainId = getOrCreateCertificateChain(
      std::exchange(request.mLogSignature->mCertificateChain,
        X509CertificateChain()));
    if (chainId)
      record.certificateChain = chainId;

    record.request = Serialization::ToCharVector(std::move(request));

    // and, finally, compensate for the checksum change:
    record.checksumCorrection = old_checksum ^ record.checksum();
    assert(record.checksum() == old_checksum);

    this->mStorage->raw.update(record);

    done++;
    if (done % 1000 == 0) {
      LOG(LOG_TAG, warning) << "  " << done;
    }
  }

  // record successful migration
  this->mStorage->raw.insert(MigrationRecord(2));
}

void TranscryptorStorage::computeChecksum(const std::string& chain,
      std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
      uint64_t& checkpoint) {
  auto position = mChecksumChains.find(chain);
  if (position == mChecksumChains.cend()) {
    throw Error("No such checksum chain");
  }

  auto result = (*position)->get(mStorage, maxCheckpoint.value_or(std::numeric_limits<int64_t>::max()));
  checksum = result.checksum;
  checkpoint = result.checkpoint;
}

std::vector<std::string> TranscryptorStorage::getChecksumChainNames() {
  std::vector<std::string> ret;
  ret.reserve(mChecksumChains.size());
  for (const auto& pair : mChecksumChains) {
    ret.push_back(pair->name());
  }
  return  ret;
}

int64_t TranscryptorStorage::getOrCreateModeSet(
    std::vector<std::string> modes) {
  std::sort(modes.begin(), modes.end());
  Sha256 hash;
  for (const auto& mode : modes) {
    hash.update(PackUint64BE(mode.size()));
    hash.update(mode);
  }
  auto key = hash.digest();

  // Work around https://github.com/fnc12/sqlite_orm/issues/245
  key = boost::algorithm::hex(key.substr(0, 16));

  // Check if the set exists
  if (auto optRec = RangeToOptional(
    mStorage->raw.iterate<ModeSetRecord>(
      where(c(&ModeSetRecord::key) == key)))) {
    return optRec->seqno;
  }

  // Apparently not --- create it
  auto set = mStorage->raw.insert(ModeSetRecord(key));
  for (auto& mode : modes)
    mStorage->raw.insert(ModeSetModeRecord(mode, set));
  return set;
}

int64_t TranscryptorStorage::getOrCreateColumnSet(
    std::vector<std::string> cols) {
  std::sort(cols.begin(), cols.end());
  Sha256 hash;
  for (const auto& col : cols) {
    hash.update(PackUint64BE(col.size()));
    hash.update(col);
  }
  auto key = hash.digest();

  // Work around https://github.com/fnc12/sqlite_orm/issues/245
  key = boost::algorithm::hex(key.substr(0, 16));

  // Check if the set exists
  if (auto optRec = RangeToOptional(
    mStorage->raw.iterate<ColumnSetRecord>(
      where(c(&ColumnSetRecord::key) == key)))) {
    return optRec->seqno;
  }

  // Apparently not --- create it
  auto set = mStorage->raw.insert(ColumnSetRecord(key));
  for (auto& col : cols)
    mStorage->raw.insert(ColumnSetColumnRecord(col, set));
  return set;
}

int64_t TranscryptorStorage::getOrCreatePseudonymSet(const std::vector<LocalPseudonym>& ps) {
  // Compute key to lookup pseudonymset
  std::vector<std::string> pps;
  pps.reserve(ps.size());
  for (auto& p : ps) {
    pps.push_back(std::string(p.pack()));
  }
  std::sort(pps.begin(), pps.end());
  auto key = Sha256().digest(
      std::accumulate(pps.begin(), pps.end(), std::string()));

  // Work around https://github.com/fnc12/sqlite_orm/issues/245
  key = boost::algorithm::hex(key.substr(0, 16));

  // Check if the set exists
  if (auto optRec = RangeToOptional(
    mStorage->raw.iterate<PseudonymSetRecord>(
      where(c(&PseudonymSetRecord::key) == key)))) {
    return optRec->seqno;
  }

  // Apparently not --- create it
  auto set = mStorage->raw.insert(PseudonymSetRecord(key));
  for (auto& p : ps)
    mStorage->raw.insert(PseudonymSetPseudonymRecord(p, set));
  return set;
}

std::optional<int64_t> TranscryptorStorage::getOrCreateCertificateChain(
    X509CertificateChain&& chain) {

  // Recall that X509CertificateChain is derived from
  //  std::list<X509Certificate>, so chain is just a list of certificates:
  //
  //   chain = cert_0, cert_1, ..., cert_N,
  //
  // where the cert_0 is the leaf certificate (and cert_N might be the root).
  // Cf. Signature::getLeafCertificate in crypto/Signature.hpp.

  // First compute the fingerprint of the chain:
  //
  //   fingerprint = sha256(cert_0) sha256(cert_1) ... sha256(cert_N)
  //
  std::vector<std::string> leafs;
  leafs.reserve(chain.size());
  std::string fingerprint;
  {
    std::ostringstream os_fingerprint;
    for (auto&& cert : chain) {
      leafs.push_back(Serialization::ToString(cert));
      os_fingerprint << Sha256().digest(leafs.back());
    }
    fingerprint = os_fingerprint.str();
  }
  auto fingerprint_it = fingerprint.begin();

  // find smallest K such that cert_K, ..., cert_N is in our database,
  // and get its seqno, which will be the parentId of the first
  // CertificateChainRecord we must insert.
  size_t K = 0;
  std::optional<int64_t> parentId;

  assert(leafs.size() == chain.size());
  assert(fingerprint.size() == 32*chain.size());

  for (; K<chain.size(); K++, fingerprint_it += 32) {
    // see if  cert_K, ..., cert_N  is our database
    std::vector<int64_t> results = this->mStorage->raw.select(
        &CertificateChainRecord::seqno, where(
          c(&CertificateChainRecord::fingerprint)
                == std::vector<char>(fingerprint_it, fingerprint.end())));

    if (results.size() == 1) {
      parentId = results[0];
      break;
    }

    if (results.size() > 1) {
      throw Error("Certificate chain with fingerprint "
          + std::string(fingerprint_it, fingerprint.end()) +
          " appears more than once!");
    }
  }
  // note that parentId might not be set here when none of the certificates
  // was in our database.  In that case:
  assert( (K==chain.size()) == (!parentId));

  // Now, roll back and insert new records.
  CertificateChainRecord chainRecord;
  while (K>0) {
    K--; fingerprint_it -= 32;

    chainRecord = CertificateChainRecord(
        std::vector<char>(leafs[K].begin(), leafs[K].end()),
        parentId,
        std::vector<char>(fingerprint_it, fingerprint.end()));

    parentId = this->mStorage->raw.insert(chainRecord);
  }

  return parentId;
}

// Retrieves the current version of the database according to the
// "Migration" table.  Returns null when no migration has been recorded,
// which is the case for the original database format (version '1').
// Creating a database from scratch is (from version 2 onwards) recorded
// as a migration too, so the result, when non-null should be >= 2.
std::optional<uint64_t> TranscryptorStorage::getCurrentVersion() {
  std::vector<MigrationRecord> records
    = this->mStorage->raw.get_all<MigrationRecord>();

  if (records.empty())
    return std::nullopt;

  // sort on timestamp;  the latest record is considered leading
  std::sort(records.begin(), records.end(),
    [](const MigrationRecord& a, const MigrationRecord& b) -> bool {
      return a.timestamp < b.timestamp;
    });

  MigrationRecord& latest = records.back();

  if (latest.toVersion <= 1) {
    std::string message = "There cannot have been a migration to version  "
      + std::to_string(latest.toVersion) + ", yet one has been recorded."
      " (The first valid migration is from version 1 to version 2.)";
    LOG(LOG_TAG, error) << message;
    throw Error(message);
  }

  if (latest.toVersion > MigrationRecord::TargetVersion) {
    // This should only happen during a rollback.
    std::string message = "The transcryptor database has version "
      + std::to_string(latest.toVersion) + ", while this transcryptor only "
      "supports versions " + std::to_string(MigrationRecord::TargetVersion)
      + " and older.";
    LOG(LOG_TAG, error) << message;
    throw Error(message);
  }

  return latest.toVersion;
}


std::string TranscryptorStorage::logTicketRequest(
    const std::vector<LocalPseudonym>& localPseudonyms,
    const std::vector<std::string>& modes,
    SignedTicketRequest2 ticketRequest,
    std::string pseudonymHash) {

  // Already compute the access group now,
  std::string accessGroup
      = ticketRequest.mLogSignature->getLeafCertificateOrganizationalUnit();
  // because we move the certificate chain from ticketRequest to its own table.
  if (!ticketRequest.mLogSignature)
    throw Error("log signature on ticket request is not set");

  auto chainId = getOrCreateCertificateChain(
    std::exchange(ticketRequest.mLogSignature->mCertificateChain,
      X509CertificateChain()));

  auto record = TicketRequestRecord(
    std::move(ticketRequest),
    getOrCreatePseudonymSet(localPseudonyms),
    getOrCreateModeSet(modes),
    std::move(pseudonymHash),
    std::move(accessGroup),
    chainId
  );
  auto id = record.id;
  mStorage->raw.insert(record);
  return id;
}

void TranscryptorStorage::logIssuedTicket(
    const std::string& id,
    const std::string& pseudonymHash,
    std::vector<std::string> columns,
    std::vector<std::string> modes,
    const std::string& accessGroup,
    Timestamp timestamp) {
  auto reqRecord = RangeToOptional(
    mStorage->raw.iterate<TicketRequestRecord>(
      where(c(&TicketRequestRecord::id) == id)));
  if (!reqRecord)
    throw Error("No TicketRequest logged with that id");
  if (reqRecord->getPseudonymHash() != pseudonymHash)
    throw Error("Pseudonyms on Ticket don't match those returned by Transcryptor");
  if (reqRecord->accessGroup != accessGroup) {
    std::ostringstream os;
    os << "Access group on ticket ("
        << std::quoted(accessGroup)
        << ") does not match access group on request ("
        << std::quoted(reqRecord->accessGroup) <<  ")";
    throw Error(os.str());
  }

  auto drift = Abs(timestamp - TimeNow());
  if (drift > 5min) {
    throw Error("Timestamp on ticket too far from current time");
  }

  if (reqRecord->modeSet != getOrCreateModeSet(modes))
    throw Error("Modes on ticket do not match modes on request");

  auto record = TicketIssueRecord(
    reqRecord->seqno,
    getOrCreateColumnSet(std::move(columns)),
    timestamp
  );
  mStorage->raw.insert(record);
}

}
