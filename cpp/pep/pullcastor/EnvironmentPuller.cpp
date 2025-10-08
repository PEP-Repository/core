#include <pep/pullcastor/EnvironmentPuller.hpp>
#include <chrono>
#include <iostream>

#include <pep/async/RxGetOne.hpp>
#include <pep/async/RxToUnorderedMap.hpp>
#include <pep/async/RxToVector.hpp>
#include <pep/pullcastor/StudyPuller.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/client/Client.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/elgamal/CurvePoint.PropertySerializer.hpp>

#include <boost/asio/io_context.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-distinct.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-window.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-zip.hpp>

namespace pep {
namespace castor {

namespace {

const int STOREDATA_WINDOW_SIZE = 100;

std::shared_ptr<CoreClient> Upcast(std::shared_ptr<Client> client) {
  return client;
}

rxcpp::observable<std::shared_ptr<CoreClient>> EnsureEnrolled(std::shared_ptr<Client> client, const std::string& token) {
  if (client->getEnrolled()) {
    return rxcpp::observable<>::just(Upcast(client));
  }
  return client->enrollUser(token)
    .map([client](EnrollmentResult) {return Upcast(client); });
}

rxcpp::observable<std::string> GetReadWritableColumnNames(std::shared_ptr<CoreClient> client) {
  return client->getAccessManagerProxy()->getAccessibleColumns(true)
    .flat_map([](const ColumnAccess& access) {
    std::set<std::string> result;
    for (const auto& group : access.columnGroups) {
      const ColumnAccess::GroupProperties& properties = group.second;
      if (std::find(properties.modes.cbegin(), properties.modes.cend(), "read") != properties.modes.cend()
        && std::find(properties.modes.cbegin(), properties.modes.cend(), "write") != properties.modes.cend()) {
        for (const auto index : properties.columns.mIndices) {
          const auto& column = access.columns[index];
          result.emplace(column);
        }
      }
    }
    return rxcpp::observable<>::iterate(result);
    })
    .distinct();
}

}

EnvironmentPuller::EnvironmentPuller(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config, bool dry, const std::optional<std::vector<std::string>>& spColumns, const std::optional<std::vector<std::string>>& sps)
  : mDry(dry), mSps(sps) {
  std::filesystem::path oauthTokenFile;
  std::filesystem::path castorAPIKeyFile;
  std::filesystem::path clientConfigFile;

  try {
    clientConfigFile = std::filesystem::canonical(config.get<std::filesystem::path>("ClientConfigFile"));
    oauthTokenFile = std::filesystem::canonical(config.get<std::filesystem::path>("OAuthTokenFile"));
    castorAPIKeyFile = std::filesystem::canonical(config.get<std::filesystem::path>("CastorAPIKeyFile"));

    auto waitPeriod = config.get<int64_t>("WaitPeriodDays") * 24 * 60 * 60 * 1000;
    mCooldownThreshold = pep::Timestamp{Timestamp().getTime() - waitPeriod};

    auto metricsFile = config.get<std::optional<std::filesystem::path>>("Metrics.TargetFile");
    if (metricsFile) {
      mMetrics = std::make_shared<Metrics>(config.get<std::string>("Metrics.JobName"), *metricsFile);
    }
    else {
      // Create a Metrics instance that doesn't write to file, so we'll be able to use it without repeated NULL checks.
      mMetrics = std::make_shared<Metrics>();
    }
  }
  catch (const std::exception& e) {
    PULLCASTOR_LOG(critical) << "Error with PullCastor configuration file: " << e.what();
    throw;
  }

  try {
    auto clientConfig = Configuration::FromFile(clientConfigFile);

    Client::Builder clientBuilder;

    clientBuilder.setCaCertFilepath(clientConfig.get<std::filesystem::path>("CACertificateFile"));
    clientBuilder.setPublicKeyData(clientConfig.get<ElgamalPublicKey>("PublicKeyData"));
    clientBuilder.setPublicKeyPseudonyms(clientConfig.get<ElgamalPublicKey>("PublicKeyPseudonyms"));
    clientBuilder.setAccessManagerEndPoint(clientConfig.get<EndPoint>("AccessManager"));
    clientBuilder.setStorageFacilityEndPoint(clientConfig.get<EndPoint>("StorageFacility"));
    clientBuilder.setKeyServerEndPoint(clientConfig.get<EndPoint>("KeyServer"));
    clientBuilder.setTranscryptorEndPoint(clientConfig.get<EndPoint>("Transcryptor"));

    clientBuilder.setIoContext(io_context);

    mClient = clientBuilder.build();
  }
  catch (const std::exception& e) {
    PULLCASTOR_LOG(critical) << "Error with client configuration file: " << e.what();
    throw;
  }

  try {
    mOauthToken = OAuthToken::ReadJson(oauthTokenFile).getSerializedForm();
  }
  catch (const std::exception& e) {
    PULLCASTOR_LOG(critical) << "Error with OAuthToken file: " << e.what();
    PULLCASTOR_LOG(critical) << "OAuthToken is being read from " << oauthTokenFile << std::endl;
    throw;
  }

  mCastor = CastorConnection::Create(castorAPIKeyFile, io_context);

  mAspects = CreateRxCache([client = mClient, token = mOauthToken, spColumns, sps]() {
    return StudyAspect::GetAll(
      EnsureEnrolled(client, token)
      .flat_map([](std::shared_ptr<CoreClient> client) {return client->getGlobalConfiguration(); })
      .flat_map([spColumns, sps](std::shared_ptr<GlobalConfiguration> gc) {
        // Get all SP definitions
        rxcpp::observable<ShortPseudonymDefinition> allowedSps = rxcpp::observable<>::iterate(gc->getShortPseudonyms());

        // If SP column names have been specified, limit to those
        if (spColumns.has_value()) {
          allowedSps = allowedSps.filter([spColumns](const ShortPseudonymDefinition& sp) {
            return std::find(spColumns->cbegin(), spColumns->cend(), sp.getColumn().getFullName()) != spColumns->cend();
          });
        }

        // If SP values have been specified, limit to columns corresponding to those
        if (sps.has_value()) {
          std::set<std::string> colNames;
          for (const auto& sp : *sps) {
            auto definition = gc->getShortPseudonymForValue(sp);
            if (definition.has_value()) {
              colNames.emplace(definition->getColumn().getFullName());
            }
          }
          allowedSps = allowedSps.filter([colNames](const ShortPseudonymDefinition& sp) {
            return colNames.find(sp.getColumn().getFullName()) != colNames.cend();
            });
        }

        // Return (possibly filtered) list of SP definitions
        return allowedSps;
        }));
    });

  mColumnNamer = CreateRxCache([client = mClient, token = mOauthToken]() {
    return EnsureEnrolled(client, token)
      .flat_map([](std::shared_ptr<CoreClient> client) {return client->getAccessManagerProxy()->getColumnNameMappings(); })
      .map([](ColumnNameMappings mappings) { return std::make_shared<ImportColumnNamer>(std::move(mappings)); });
    });

  mStudiesBySlug = CreateRxCache([castor = mCastor]() {
    return castor->getStudies()
      .op(RxToUnorderedMap([](std::shared_ptr<Study> study) {return study->getSlug(); }));
    });
}

rxcpp::observable<size_t> EnvironmentPuller::pull() {
  auto startTime = std::chrono::steady_clock::now();

  // Register a callback so we can detect duplicate Castor requests.
  // Use a weak_ptr to prevent circular references between EnvironmentPuller and its CastorClient::Connection
  mCastorOnRequestSubscription = mCastor->onRequest.subscribe([weak = WeakFrom(*this)](std::shared_ptr<const HTTPRequest> request) {
    auto self = weak.lock();
    if (self != nullptr) {
      self->onCastorRequest(request);
    }
  });

  auto read = std::make_shared<size_t>(0U);
  auto written = std::make_shared<size_t>(0U);
  auto self = SharedFrom(*this);

  if (mDry) {
    PULLCASTOR_LOG(info) << "Performing a dry run: no data will be stored in PEP";
  }
  else {
    PULLCASTOR_LOG(info) << "Performing an import run: PEP will be updated with data retrieved from Castor";
  }

  // Perform the actual pulling.
  return StudyPuller::CreateChildrenFor(self) // Create study pullers
    .concat_map([](std::shared_ptr<StudyPuller> study) {return study->getStorableContent(); }) // Get Castor content from each study puller
    .flat_map([self, read](std::shared_ptr<StorableCellContent> castor) { // Get StoreData2Entry items for Castor content that PEP doesn't have
    ++*read;
    return self->getStorageUpdate(castor);
    })
    .window(STOREDATA_WINDOW_SIZE) // Process StoreData2Entry items in batches
    .concat_map([](rxcpp::observable<StoreData2Entry> batch) { return batch.op(RxToVector()); }) // Get this batch's items as a vector
    .flat_map([self](std::shared_ptr<std::vector<StoreData2Entry>> batch) {return self->processBatchToStore(batch); }) // Store the items
    .tap( // Perform housekeeping
      [self, written](size_t count) {
        self->mMetrics->storedEntries_count.Increment(static_cast<double>(count));
        (*written) += count;
      },
      [self](std::exception_ptr) {self->mMetrics->uncaughtExceptions_count.Increment(); },
      [self, read, written, startTime]() {
        self->mMetrics->importDuration_seconds.Set(std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count()); // in seconds
        PULLCASTOR_LOG(info) << "Added/updated " << *written << " of " << *read << " entries";
      }
    );
}

rxcpp::observable<size_t> EnvironmentPuller::processBatchToStore(std::shared_ptr<std::vector<StoreData2Entry>> batch) {
  if (mDry) {
    return rxcpp::observable<>::just(batch->size());
  }

  return mClient->storeData2(*batch)
    .map([](DataStorageResult2 dataStorageResult) { return dataStorageResult.mIds.size(); });
}

rxcpp::observable<StoreData2Entry> EnvironmentPuller::getStorageUpdate(std::shared_ptr<StorableCellContent> castor) {
  auto storing = std::make_shared<bool>(false);
  return this->getStoredData()
    .flat_map([castor](std::shared_ptr<StoredData> stored) { return stored->getUpdateEntry(castor); })
    .tap(
      [storing](const StoreData2Entry&) { *storing = true; },
      [](std::exception_ptr) {},
      [castor, storing]() {
        auto description = (*storing) ? "Storing" : "Skipping";
        PULLCASTOR_LOG(info) << description << " participant " << castor->getColumnBoundParticipantId().getParticipantId() << " column " << castor->getColumn();
      }
    );
}

bool EnvironmentPuller::Pull(const Configuration& config, bool dry, const std::optional<std::vector<std::string>>& spColumns, const std::optional<std::vector<std::string>>& sps) {
  auto result = std::make_shared<bool>(false);
  auto io_context = std::make_shared<boost::asio::io_context>();

  PULLCASTOR_LOG(info) << "Starting castor pull";
  auto instance = EnvironmentPuller::Create(io_context, config, dry, spColumns, sps);
  instance->pull()
    .subscribe(
      [](size_t count) { PULLCASTOR_LOG(debug) << "Written " << count << " entries"; },
      [io_context](std::exception_ptr ep) {
        PULLCASTOR_LOG(error) << "Exception occured while writing Castor data to PEP: " << rxcpp::rxu::what(ep);
        io_context->stop();
      },
      [io_context, result]() {
        PULLCASTOR_LOG(info) << "Done pulling Castor data";
        io_context->stop();
        *result = true;
      });


  io_context->run();

  // Discover circular dependencies that'll prevent our EnvironmentPuller's destructor from running.
  // See https://gitlab.pep.cs.ru.nl/pep/ppp-config/-/issues/90#note_32950
  assert(instance.use_count() == 1);

  return *result;
}

rxcpp::observable<std::shared_ptr<CoreClient>> EnvironmentPuller::getClient() {
  return EnsureEnrolled(mClient, mOauthToken);
}

void EnvironmentPuller::onCastorRequest(std::shared_ptr<const HTTPRequest> request) {
  std::string uri(request->uri().buffer());
  auto position = mCastorRequests.find(uri);
  if (position == mCastorRequests.cend()) {
    mCastorRequests.emplace(std::make_pair(uri, 1U));
  }
  else {
    position->second++;
    PULLCASTOR_LOG(debug) << "Sending Castor request no. " << position->second << " to " << uri;
  }
}

rxcpp::observable<std::shared_ptr<std::vector<std::string>>> EnvironmentPuller::getShortPseudonymColumns() {
  return this->getStudyAspects()
    .map([](const StudyAspect& aspect) {return aspect.getShortPseudonymColumn(); })
    .distinct()
    .op(RxToVector());
}

rxcpp::observable<std::shared_ptr<std::vector<std::string>>> EnvironmentPuller::getDeviceHistoryColumns() {
  return this->getStudyAspects()
    .map([](const StudyAspect& aspect) {return aspect.getStorage()->getWeekOffsetDeviceColumn(); })
    .filter([](const std::string& dhColumn) {return !dhColumn.empty(); })
    .distinct()
    .op(RxToVector());
}

rxcpp::observable<std::shared_ptr<std::vector<std::string>>> EnvironmentPuller::getDataStorageColumns() {
  return this->getStudyAspects()
    .map([](const StudyAspect& aspect) {return aspect.getStorage()->getDataColumn(); })
    .distinct()
    .op(RxToVector())
    .flat_map([client = mClient](std::shared_ptr<std::vector<std::string>> prefixes) {
    return GetReadWritableColumnNames(client)
      .filter([prefixes](const std::string& column) {
        for (const auto& prefix : *prefixes) {
          if (column.starts_with(prefix)) {
            return true;
          }
        }
        return false;
      })
      .op(RxToVector());
    });
}

rxcpp::observable<std::shared_ptr<std::vector<PolymorphicPseudonym>>> EnvironmentPuller::getPps() {
  if (!mSps.has_value()) {
    return rxcpp::observable<>::just(std::make_shared<std::vector<PolymorphicPseudonym>>()); // Return an empty vector rather than an empty observable
  }
  return rxcpp::observable<>::iterate(*mSps)
    .flat_map([client = mClient](const std::string& sp) {return client->findPPforShortPseudonym(sp); })
    .op(RxToVector());
}

rxcpp::observable<std::shared_ptr<StoredData>> EnvironmentPuller::getStoredData() {
  if (mStoredData != nullptr) {
    return rxcpp::observable<>::just(mStoredData);
  }
  PULLCASTOR_LOG(info) << "Retrieving stored data from PEP";
  return this->getClient().op(RxGetOne("client"))
    .zip(
      this->getShortPseudonymColumns().op(RxGetOne("short pseudonym columns")),
      this->getDeviceHistoryColumns().op(RxGetOne("device history columns")),
      this->getDataStorageColumns().op(RxGetOne("data storage columns")),
      this->getPps().op(RxGetOne("participant polymorphic pseudonyms"))
    )
    .flat_map([self = SharedFrom(*this)](const auto& context) {
    std::shared_ptr<CoreClient> client = std::get<0>(context);
    std::shared_ptr<std::vector<std::string>> spColumns = std::get<1>(context);
    std::shared_ptr<std::vector<std::string>> dhColumns = std::get<2>(context);
    std::shared_ptr<std::vector<std::string>> dataColumns = std::get<3>(context);
    std::shared_ptr<std::vector<PolymorphicPseudonym>> pps = std::get<4>(context);

    auto nonSpColumns = MakeSharedCopy(*dhColumns);
    std::copy(dataColumns->cbegin(), dataColumns->cend(), std::back_inserter(*nonSpColumns));

    return StoredData::Load(client, pps, spColumns, nonSpColumns)
      .flat_map([self](std::shared_ptr<StoredData> stored) {
      self->mStoredData = stored;
      PULLCASTOR_LOG(info) << "Retrieved stored data from PEP";
      return self->getStoredData();
      });
    });
}

rxcpp::observable<std::shared_ptr<Study>> EnvironmentPuller::getStudyBySlug(const std::string& slug) {
  return mStudiesBySlug->observe()
    .map([slug](std::shared_ptr<StudiesBySlug> studies) {return studies->at(slug); });
}

}
}
