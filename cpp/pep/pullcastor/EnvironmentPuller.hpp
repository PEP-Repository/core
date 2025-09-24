#pragma once

#include <pep/utils/Log.hpp>
#include <pep/async/RxCache.hpp>
#include <pep/client/Client_fwd.hpp>
#include <pep/castor/CastorConnection.hpp>
#include <pep/castor/ImportColumnNamer.hpp>
#include <pep/pullcastor/Metrics.hpp>
#include <pep/pullcastor/StoredData.hpp>
#include <pep/pullcastor/StudyAspect.hpp>

#include <string>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <optional>
#include <vector>

namespace pep {
namespace castor {

/*!
  * \brief Top level implementor for pepPullCastor utility: imports all data from appropriate Castor studies into PEP.
  */
class EnvironmentPuller : public std::enable_shared_from_this<EnvironmentPuller>, private SharedConstructor<EnvironmentPuller> {
  friend class SharedConstructor<EnvironmentPuller>;

private:
  using StudiesBySlug = std::unordered_map<std::string, std::shared_ptr<Study>>;

  bool mDry;
  std::optional<std::vector<std::string>> mSps;
  Timestamp mCooldownThreshold;
  std::shared_ptr<Client> mClient;
  std::string mOauthToken;
  std::shared_ptr<CastorConnection> mCastor;
  EventSubscription mCastorOnRequestSubscription;
  std::shared_ptr<Metrics> mMetrics;
  std::unordered_map<std::string, size_t> mCastorRequests;
  std::shared_ptr<StoredData> mStoredData;

  std::shared_ptr<RxCache<StudyAspect>> mAspects;
  std::shared_ptr<RxCache<std::shared_ptr<ImportColumnNamer>>> mColumnNamer;
  std::shared_ptr<RxCache<std::shared_ptr<StudiesBySlug>>> mStudiesBySlug;

  EnvironmentPuller(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config, bool dry, const std::optional<std::vector<std::string>>& spColumns, const std::optional<std::vector<std::string>>& sps);

  /*!
  * \brief Implementor for the static Pull function.
  * \return An observable emitting numbers of items written to PEP's storage facility. Note that multiple numbers may be emitted.
  */
  rxcpp::observable<size_t> pull();

  /*!
  * \brief Given data that's present in Castor, produces a StoreData2Entry if that data needs to be imported into PEP.
  * \param castor The data retrieved from Castor.
  * \return An observable emitting a StoreData2Entry if Castor data needs to be written to PEP's storage facility. The observable is empty if the Castor data does not need writing.
  */
  rxcpp::observable<StoreData2Entry> getStorageUpdate(std::shared_ptr<StorableCellContent> castor);

  /*!
  * \brief If not performing a dry run, stores the specified entries in PEP.
  * \param batch The entries to store.
  * \return (An observable emitting) the number of stored items.
  * \remark When performing a dry run, doesn't store the entries but only returns the number of entries that would have been sent to Storage Facility.
  */
  rxcpp::observable<size_t> processBatchToStore(std::shared_ptr<std::vector<StoreData2Entry>> batch);

  /*!
  * \brief Utility function that ensures that the Client instance is enrolled.
  * \return An observable emitting a CoreClient instance that has been enrolled with the PEP system.
  */
  rxcpp::observable<std::shared_ptr<CoreClient>> getClient();

  /*!
  * \brief Private helper function.
  * \return An observable emitting (a single vector containing) names of short pseudonym columns associated with Castor import.
  */
  rxcpp::observable<std::shared_ptr<std::vector<std::string>>> getShortPseudonymColumns();

  /*!
  * \brief Private helper function.
  * \return An observable emitting (a single vector containing) names of device history columns associated with Castor import.
  */
  rxcpp::observable<std::shared_ptr<std::vector<std::string>>> getDeviceHistoryColumns();

  /*!
  * \brief Private helper function.
  * \return An observable emitting (a single vector containing) names of columns where Castor import can store its data.
  */
  rxcpp::observable<std::shared_ptr<std::vector<std::string>>> getDataStorageColumns();

  /*!
  * \brief Private helper function.
  * \return An observable emitting (a single vector containing) polymorphic pseudonyms for the short pseudonyms specified to the constructor.
  * \remark The observable will be empty if no SPs have been specified to the constructor.
  */
  rxcpp::observable<std::shared_ptr<std::vector<PolymorphicPseudonym>>> getPps();

  /*!
  * \brief Callback that's invoked when a Castor request is (being?) sent off.
  * \param request The request being sent.
  */
  void onCastorRequest(std::shared_ptr<const HTTPRequest> request);

public: // To be called from pepPullCastor('s main function)
  /*!
  * \brief Imports Castor data for the PEP system associated with the specified configuration file.
  * \param config The PEP (Client)Config.json file
  * \param dry Whether or not to perform a dry run.
  * \param spColumns If specified, limit processing to these short pseudonym columns.
  * \param sps If specified, limit processing to these short pseudonyms.
  * \return TRUE if the import was completed successfully; FALSE if not.
  */
  static bool Pull(const Configuration& config, bool dry, const std::optional<std::vector<std::string>>& spColumns, const std::optional<std::vector<std::string>>& sps);

public: // Functions providing context data to child pullers

  /*!
  * \brief Produces (an observable emitting) study aspects that should be pulled from Castor.
  * \return The aspects to retrieve from Castor.
  */
  inline rxcpp::observable<StudyAspect> getStudyAspects() { return mAspects->observe(); }

  /*!
  * \brief Produces an optional set of short pseudonyms (i.e. Castor participant IDs) that processing should be limited to.
  * \return Short pseudonyms to process, or std::nullopt if all participants should be processed.
  */
  inline const std::optional<std::vector<std::string>>& getShortPseudonymsToProcess() const noexcept { return mSps; }

  /*!
  * \brief Produces (an observable emitting) a representation of the data currently stored in PEP.
  * \return The (import-related) data stored in PEP.
  */
  rxcpp::observable<std::shared_ptr<StoredData>> getStoredData();

  /*!
  * \brief Produces (an observable emitting) the import column namer for this PEP environment.
  * \return The import column namer for this environment.
  */
  inline rxcpp::observable<std::shared_ptr<ImportColumnNamer>> getImportColumnNamer() { return mColumnNamer->observe(); }

  /*!
  * \brief Produces (an observable emitting) the Castor Study object associated with the specified slug.
  * \param slug The study slug to find
  * \return The Study object that has the specified slug.
  * \remark Produces the Study object from cached data (i.e. without sending a new Castor request), as opposed to CastorClient::Connection::getStudyBySlug.
  */
  rxcpp::observable<std::shared_ptr<Study>> getStudyBySlug(const std::string& slug);

  /*!
  * \brief Produces (an observable emitting) the Castor connection associated with this environment.
  * \return The Castor connection from which to retrieve Castor data.
  */
  inline std::shared_ptr<CastorConnection> getCastor() const noexcept { return mCastor; }

  /*!
  * \brief Produces the timestamp that corresponds with the configured cooldown period. Data requiring cooldown should be older than this timestamp.
  * \return The timestamp marking the end of the cooldown period for affected data.
  */
  inline const Timestamp& getCooldownThreshold() const noexcept { return mCooldownThreshold; }
};

}
}
