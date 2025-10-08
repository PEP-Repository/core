#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/cli/MultiCellQuery.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/async/RxCache.hpp>
#include <pep/async/RxConcatenateVectors.hpp>
#include <pep/async/RxGetOne.hpp>
#include <pep/async/RxToSet.hpp>
#include <pep/async/RxToVector.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/ChronoUtil.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-distinct.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-zip.hpp>

#include <boost/algorithm/string/split.hpp>

using namespace pep::cli;

namespace {

template <typename TKey, typename TValue>
rxcpp::observable<std::shared_ptr<std::map<TKey, TValue>>> Batch(size_t batchSize, std::shared_ptr<std::map<TKey, TValue>> all) {
  return pep::CreateObservable<std::shared_ptr<std::map<TKey, TValue>>>([batchSize, all](rxcpp::subscriber<std::shared_ptr<std::map<TKey, TValue>>> subscriber) {
    auto notified = false;
    auto batch = std::make_shared<std::map<TKey, TValue>>();
    for (const auto& entry : *all) {
      [[maybe_unused]] auto emplaced = batch->emplace(entry).second;
      assert(emplaced);
      if (batch->size() == batchSize) {
        subscriber.on_next(batch);
        notified = true;
        batch = std::make_shared<std::map<TKey, TValue>>();
      }
    }

    if (batch->size() != 0U || !notified) {
      subscriber.on_next(batch);
    }
    subscriber.on_completed();
    });
}


class CommandFileExtension : public ChildCommandOf<CliApplication> {
public:
  explicit CommandFileExtension(CliApplication& parent)
    : ChildCommandOf<CliApplication>("file-extension", "Manipulate file extensions", parent) {
  }

protected:
  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override;
};

class FileExtensionRequiringChildCommand : public ChildCommandOf<CommandFileExtension> {
private:
  std::weak_ptr<pep::CoreClient> mClient;
  std::shared_ptr<pep::RxCache<std::shared_ptr<pep::ColumnAccess>>> mMetaReadableColumnGroups;
  std::shared_ptr<pep::RxCache<std::string>> mAccessibleParticipantGroups;

protected:
  using ColumnExtensions = std::map<std::string, std::string>;

  rxcpp::observable<std::shared_ptr<pep::ColumnAccess>> getMetaReadableColumnGroups(std::shared_ptr<pep::CoreClient> client) {
    if (mMetaReadableColumnGroups == nullptr) {
      mMetaReadableColumnGroups = pep::CreateRxCache([client]() {
        return client->getAccessManagerProxy()->getAccessibleColumns(true, { "read-meta" })
          .op(pep::RxGetOne("column access specification"))
          .map([](const pep::ColumnAccess& access) {return pep::MakeSharedCopy(access); });
        });
      if (mClient.lock() == nullptr) {
        mClient = client;
      }
    }

    assert(mClient.lock() == client);
    return mMetaReadableColumnGroups->observe();
  }

  rxcpp::observable<std::string> getAccessibleParticipantGroups(std::shared_ptr<pep::CoreClient> client) {
    if (mAccessibleParticipantGroups == nullptr) {
      mAccessibleParticipantGroups = pep::CreateRxCache([client]() {
        return client->getAccessManagerProxy()->getAccessibleParticipantGroups(true)
          .flat_map([](const pep::ParticipantGroupAccess& access) {
          std::set<std::string> result;
          for (const auto& group : access.participantGroups) {
            const auto& modes = group.second;
            if (std::find(modes.cbegin(), modes.cend(), "access") != modes.cend()
              && std::find(modes.cbegin(), modes.cend(), "enumerate") != modes.cend()) {
              result.emplace(group.first);
            }
          }
          return rxcpp::observable<>::iterate(result);
            })
          .distinct()
          .op(pep::RxToSet())
          .flat_map([](std::shared_ptr<std::set<std::string>> groups) -> rxcpp::observable<std::string> {
          if (groups->find("*") != groups->cend()) {
            return rxcpp::observable<>::just(std::string("*"));
          }
          return rxcpp::observable<>::iterate(*groups);
          });
        });
      if (mClient.lock() == nullptr) {
        mClient = client;
      }
    }

    assert(mClient.lock() == client);
    return mAccessibleParticipantGroups->observe();
  }

  rxcpp::observable<std::string> getMetaReadableColumns(std::shared_ptr<pep::CoreClient> client) {
    return this->getMetaReadableColumnGroups(client)
      .flat_map([](std::shared_ptr<pep::ColumnAccess> access) { return rxcpp::observable<>::iterate(access->columns); })
      .distinct();
  }

  rxcpp::observable<std::string> getColumnsInGroupIfMetaReadable(std::shared_ptr<pep::CoreClient> client, const std::string& group) {
    return this->getMetaReadableColumnGroups(client)
      .flat_map([group](std::shared_ptr<pep::ColumnAccess> access) {
      std::vector<std::string> columns;
      auto position = access->columnGroups.find(group);
      if (position == access->columnGroups.cend()) {
        std::cerr << "Skipping inaccessible column group " << group << std::endl;
      }
      else {
        const auto& indices = position->second.columns.mIndices;
        columns.reserve(indices.size());
        for (auto i : indices) {
          columns.emplace_back(access->columns[i]);
        }
      }
      return rxcpp::observable<>::iterate(columns);
        });
  }

  static rxcpp::observable<std::shared_ptr<ColumnExtensions>> GetGlobalConfigurationColumnExtensions(std::shared_ptr<pep::CoreClient> client) {
    return client->getGlobalConfiguration()
      .op(pep::RxGetOne("global configuration"))
      .map([](std::shared_ptr<pep::GlobalConfiguration> config) {
      auto result = std::make_shared<ColumnExtensions>();
      for (const auto& sp : config->getShortPseudonyms()) {
        result->emplace(sp.getColumn().getFullName(), ".txt");
      }
      for (const auto& device : config->getDevices()) {
        result->emplace(device.columnName, ".json");
      }
      return result;
        });
  }

  rxcpp::observable<std::shared_ptr<ColumnExtensions>> getVisitAssessorColumnExtensions(std::shared_ptr<pep::CoreClient> client) {
    return this->getColumnsInGroupIfMetaReadable(client, "VisitAssessors")
      .reduce(
        std::make_shared<ColumnExtensions>(),
        [](std::shared_ptr<ColumnExtensions> result, const std::string& column) {
          result->emplace(column, ".txt");
          return result;
        });
  }

  rxcpp::observable<std::shared_ptr<ColumnExtensions>> getCastorColumnExtensions(std::shared_ptr<pep::CoreClient> client) {
    return this->getColumnsInGroupIfMetaReadable(client, "Castor")
      .reduce(
        std::make_shared<ColumnExtensions>(),
        [](std::shared_ptr<ColumnExtensions> result, const std::string& column) {
          std::vector<std::string> parts;
          boost::split(parts, column, [](char c) {return c == '.'; });
          size_t nofParts = parts.size();
          if (nofParts >= 2
            && parts[nofParts - 2U].starts_with("AnswerSet")
            && parts[nofParts - 1U] == "WeekNumber") {
            result->emplace(column, ".txt");
          }
          else {
            result->emplace(column, ".json");
          }
          return result;
        });
  }

  rxcpp::observable<std::shared_ptr<ColumnExtensions>> getWellKnownColumnExtensions(std::shared_ptr<pep::CoreClient> client) {
    auto hardcoded = std::make_shared<ColumnExtensions>();
    hardcoded->emplace("ParticipantIdentifier", ".txt");
    hardcoded->emplace("ParticipantInfo", ".json");
    hardcoded->emplace("StudyContexts", ".csv");
    hardcoded->emplace("IsTestParticipant", ".txt");

    return rxcpp::observable<>::just(hardcoded)
      .concat(GetGlobalConfigurationColumnExtensions(client))
      .concat(this->getVisitAssessorColumnExtensions(client))
      .concat(this->getCastorColumnExtensions(client))
      .reduce(
        std::make_shared<ColumnExtensions>(),
        [](std::shared_ptr<ColumnExtensions> all, std::shared_ptr<ColumnExtensions> sub) {
          for (auto entry : *sub) {
            const auto& key = entry.first;
            if (all->find(key) != all->cend()) {
              throw std::runtime_error("Multiple extensions specified for column " + key);
            }
            [[maybe_unused]] auto emplaced = all->emplace(entry).second;
            assert(emplaced);
          }
          return all;
        })
      .as_dynamic() // Reduce compiler memory usage
      .zip(this->getMetaReadableColumns(client).op(pep::RxToSet()))
      .map([](const auto& context) {
        std::shared_ptr<ColumnExtensions> required = std::get<0>(context);
        std::shared_ptr<std::set<std::string>> accessible = std::get<1>(context);
        auto i = required->begin();
        while (i != required->end()) {
          if (accessible->find(i->first) == accessible->cend()) {
            std::cerr << "Skipping inaccessible column " << i->first << std::endl;
            i = required->erase(i);
          }
          else {
            ++i;
          }
        }
        return required;
        });
  }

  class Update {
  private:
    pep::StoreMetadata2Entry mStoreEntry;
    pep::LocalPseudonym mParticipantAlias;
    std::optional<std::string> mPreviousExtension;

    static std::optional<std::string> GetExtension(const pep::EnumerateResult& enumResult) {
      auto position = enumResult.mMetadata.extra().find("fileExtension");
      if (position == enumResult.mMetadata.extra().cend()) {
        return std::nullopt;
      }
      return position->second.plaintext();
    }


    Update(const pep::EnumerateResult& enumResult, const std::optional<std::string>& currentExtension, const std::string& correctExtension)
      : mStoreEntry(pep::MakeSharedCopy(enumResult.mLocalPseudonyms->mPolymorphic), enumResult.mColumn),
      mParticipantAlias(*enumResult.mAccessGroupPseudonym),
      mPreviousExtension(currentExtension) {
      assert(mPreviousExtension == GetExtension(enumResult));

      // Initialize the storage entry with current metadata values (from the entry that we'll overwrite)
      mStoreEntry.mXMetadata = enumResult.mMetadata.extra();
      // Overwrite file extension entry with the correct value
      mStoreEntry.mXMetadata["fileExtension"] = pep::MetadataXEntry::FromPlaintext(correctExtension, false, false);
    }

  public:
    const pep::StoreMetadata2Entry& getStoreEntry() const noexcept { return mStoreEntry; }
    const pep::LocalPseudonym& getParticipantAlias() const noexcept { return mParticipantAlias; }
    const std::optional<std::string>& getPreviousExtension() const noexcept { return mPreviousExtension; }

    std::string getAssignedExtension() const {
      auto position = mStoreEntry.mXMetadata.find("fileExtension");
      assert(position != mStoreEntry.mXMetadata.cend());
      return position->second.plaintext();
    }

    static std::optional<Update> GetFor(const pep::EnumerateResult& enumResult, const std::string& requiredExtension) {
      auto previousExtension = GetExtension(enumResult);
      if (previousExtension == requiredExtension) { // This EnumerateResult already has the correct extension
        return std::nullopt;
      }

      return Update(enumResult, previousExtension, requiredExtension);
    }
  };

  static void ReportParticipantAndColumn(std::ostream& destination, const pep::LocalPseudonym& participantAlias, const std::string& column) {
    destination << "participant " << participantAlias.text()
      << ", column " << column;
  }

  static void ReportParticipantAndColumn(std::ostream& destination, const pep::EnumerateResult& enumResult) {
    return ReportParticipantAndColumn(destination, *enumResult.mAccessGroupPseudonym, enumResult.mColumn);
  }

  static void ReportParticipantAndColumn(std::ostream& destination, const Update& update) {
    return ReportParticipantAndColumn(destination, update.getParticipantAlias(), update.getStoreEntry().mColumn);
  }

private:
  std::optional<Update> getUpdateFor(const pep::EnumerateResult& enumResult, const ColumnExtensions& requiredExtensions) {
    auto verbose = this->getParameterValues().has("verbose");

    auto required = requiredExtensions.find(enumResult.mColumn);
    if (required == requiredExtensions.cend()) { // This column has no associated expected file extension
      if (verbose) {
        std::cout << "Skipping ";
        ReportParticipantAndColumn(std::cout, enumResult);
        std::cout << " (no specific extension required)\n";
      }
      return std::nullopt;
    }

    const std::string& correctExtension = required->second;
    auto update = Update::GetFor(enumResult, correctExtension);
    if (verbose && !update.has_value()) {
      std::cout << "Skipping ";
      ReportParticipantAndColumn(std::cout, enumResult);
      std::cout << " (correct " << correctExtension << " extension already present)\n";
    }
    return update;

  }

protected:
  FileExtensionRequiringChildCommand(const std::string& name, const std::string& description, CommandFileExtension& parent)
    : ChildCommandOf<CommandFileExtension>(name, description, parent) {
  }

  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandFileExtension>::getSupportedParameters()
      + pep::commandline::Parameter("report-progress", "Produce progress status messages")
      + pep::commandline::Parameter("verbose", "Produce additional output").shorthand('v');
  }

  virtual rxcpp::observable<std::shared_ptr<ColumnExtensions>> getRequiredColumnExtensions(std::shared_ptr<pep::CoreClient> client) = 0; // Must return a single item
  virtual rxcpp::observable<std::string> getParticipantGroupsToProcess(std::shared_ptr<pep::CoreClient> client) = 0;
  virtual rxcpp::observable<std::shared_ptr<std::vector<pep::PolymorphicPseudonym>>> getPpsToProcess(std::shared_ptr<pep::CoreClient> client) = 0; // Must return a single item
  virtual rxcpp::observable<bool> processUpdates(std::shared_ptr<pep::CoreClient> client, const std::vector<Update>& updates) = 0;

  int execute() override {
    auto ownResult = pep::MakeSharedCopy(true);
    auto connectivityResult = this->executeEventLoopFor([this, ownResult](std::shared_ptr<pep::CoreClient> client) {
      return this->getParticipantGroupsToProcess(client).op(pep::RxToVector())
        .zip(this->getPpsToProcess(client).op(pep::RxGetOne("set of polymorphic pseudonyms to process")))
        .flat_map([this, client, ownResult](const auto& context) {
        std::shared_ptr<std::vector<std::string>> pgs = std::get<0>(context);
        std::shared_ptr<std::vector<pep::PolymorphicPseudonym>> pps = std::get<1>(context);

        class Counts {
        private:
          std::optional<std::chrono::steady_clock::time_point> mStartTime;
          size_t mTotalColumns = 0U;
          size_t mColumnsSeen = 0U;
          size_t mCellsSeen = 0U;
          size_t mUpdatesSeen = 0U;

        public:
          void start(size_t totalColumns) {
            assert(!mStartTime.has_value());
            mStartTime = std::chrono::steady_clock::now();
            mTotalColumns = totalColumns;
          }

          void processingColumns(size_t count) {
            mColumnsSeen += count;
          }

          void processingCells(size_t count) {
            mCellsSeen += count;
          }

          void processingUpdates(size_t count) {
            mUpdatesSeen += count;
          }

          void reportProgress() const {
            assert(mStartTime.has_value());
            if (mTotalColumns != 0U && mColumnsSeen < mTotalColumns) {
              double completed = static_cast<double>(mColumnsSeen) / static_cast<double>(mTotalColumns);
              assert(completed >= 0.0);
              assert(completed <= 1.0);
              auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - *mStartTime).count();
              double total = elapsed / completed;
              double remaining = total - elapsed;

              auto flags = std::cout.flags();
              PEP_DEFER(std::cout.flags(flags));
              std::cout << std::setprecision(1) << std::fixed << (completed * 100) << "% done"
                << "; approximately " << pep::chrono::ToString(std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(remaining))) << " remaining"
                << std::endl;
            }
            else {
              std::cout << mUpdatesSeen << " assignable out of " << mCellsSeen << " total cells processed in " << pep::chrono::ToString(std::chrono::steady_clock::now() - *mStartTime) << std::endl;
            }
          }
        };

        auto counts = std::make_shared<Counts>();

        return this->getRequiredColumnExtensions(client)
          .concat_map([counts](std::shared_ptr<ColumnExtensions> columnExtensions) {
          counts->start(columnExtensions->size());
          return Batch(100, columnExtensions);
            })
          .concat_map([this, client, ownResult, pgs, pps, counts](std::shared_ptr<ColumnExtensions> columnExtensions) {
          counts->processingColumns(columnExtensions->size());
          pep::requestTicket2Opts ticketRequest;
          ticketRequest.modes.emplace_back("read-meta");
          ticketRequest.includeAccessGroupPseudonyms = true;
          ticketRequest.participantGroups = *pgs;
          ticketRequest.pps = *pps;

          ticketRequest.columns.reserve(columnExtensions->size());
          std::transform(columnExtensions->cbegin(), columnExtensions->cend(), std::back_inserter(ticketRequest.columns), [](const auto& pair) {return pair.first; });

          return client->requestTicket2(ticketRequest)
            .flat_map([client](const pep::IndexedTicket2& ticket) {return client->enumerateData2(ticket.getTicket()); })
            .op(pep::RxConcatenateVectors())
            .flat_map([this, client, ownResult, columnExtensions, counts](std::shared_ptr<std::vector<pep::EnumerateResult>> enumResults) {
            counts->processingCells(enumResults->size());
            std::vector<Update> updates;
            updates.reserve(enumResults->size());
            for (const auto& enumResult : *enumResults) {
              auto update = this->getUpdateFor(enumResult, *columnExtensions);
              if (update.has_value()) {
                updates.emplace_back(*update);
              }
            }
            counts->processingUpdates(updates.size());
            std::cout.flush(); // Flush getUpdateFor method's verbose messages about un-updated entries
            return this->processUpdates(client, updates)
              .map([this, ownResult, counts](bool result) {
              if (this->getParameterValues().has("report-progress")) {
                counts->reportProgress();
              }
              if (!result) {
                *ownResult = false;
              }
              return pep::FakeVoid();
                });
              });
            });
          });
      });

    if (connectivityResult != 0) {
      return connectivityResult;
    }
    return (*ownResult) ? EXIT_SUCCESS : EXIT_FAILURE;
  }
};

class CommandFileExtensionValidate : public FileExtensionRequiringChildCommand {
public:
  explicit CommandFileExtensionValidate(CommandFileExtension& parent)
    : FileExtensionRequiringChildCommand("validate", "Validate existing file extensions", parent) {
  }

protected:
  rxcpp::observable<std::shared_ptr<ColumnExtensions>> getRequiredColumnExtensions(std::shared_ptr<pep::CoreClient> client) override {
    return this->getWellKnownColumnExtensions(client);
  }

  rxcpp::observable<std::string> getParticipantGroupsToProcess(std::shared_ptr<pep::CoreClient> client) override {
    return this->getAccessibleParticipantGroups(client);
  }

  rxcpp::observable<std::shared_ptr<std::vector<pep::PolymorphicPseudonym>>> getPpsToProcess(std::shared_ptr<pep::CoreClient> client) override {
    return rxcpp::observable<>::just(std::make_shared<std::vector<pep::PolymorphicPseudonym>>());
  }

  rxcpp::observable<bool> processUpdates(std::shared_ptr<pep::CoreClient> client, const std::vector<Update>& updates) override {
    auto result = true;

    for (const auto& update : updates) {
      std::cerr << "Expected extension " << update.getAssignedExtension() << " for ";
      ReportParticipantAndColumn(std::cerr, update);
      std::cerr << " but found ";
      const auto& previous = update.getPreviousExtension();
      if (previous.has_value()) {
        std::cerr << *previous << " instead";
      }
      else
      {
        std::cerr << "none";
      }
      std::cerr << '\n';
      result = false;
    }

    std::cerr.flush();
    return rxcpp::observable<>::just(result);
  }
};

class FileExtensionAssigningChildCommand : public FileExtensionRequiringChildCommand {
protected:
  FileExtensionAssigningChildCommand(const std::string& name, const std::string& description, CommandFileExtension& parent)
    : FileExtensionRequiringChildCommand(name, description, parent) {
  }

  rxcpp::observable<bool> processUpdates(std::shared_ptr<pep::CoreClient> client, const std::vector<Update>& updates) override {
    if (updates.empty()) {
      return rxcpp::observable<>::just(true);
    }

    std::vector<pep::StoreMetadata2Entry> storeEntries;
    storeEntries.reserve(updates.size());
    std::transform(updates.cbegin(), updates.cend(), std::back_inserter(storeEntries), [verbose = this->getParameterValues().has("verbose")](const Update& update) {
      if (verbose) {
        const auto& previous = update.getPreviousExtension();
        if (previous.has_value()) {
          std::cout << "Overwriting file extension " << *previous << " with ";
        }
        else {
          std::cout << "Assigning file extension ";
        }
        std::cout << update.getAssignedExtension() << " for ";
        ReportParticipantAndColumn(std::cout, update);
        std::cout << '\n';
      }
      return update.getStoreEntry();
      });
    std::cout.flush();

    return client->updateMetadata2(storeEntries)
      .op(pep::RxGetOne("metadata update result"))
      .map([](const pep::DataStorageResult2&) {return true; });
  }
};

class CommandFileExtensionAutoAssign : public FileExtensionAssigningChildCommand {
public:
  explicit CommandFileExtensionAutoAssign(CommandFileExtension& parent)
    : FileExtensionAssigningChildCommand("auto-assign", "Assign file extensions to cells in well-known columns", parent) {
  }

protected:
  rxcpp::observable<std::shared_ptr<ColumnExtensions>> getRequiredColumnExtensions(std::shared_ptr<pep::CoreClient> client) override {
    return this->getWellKnownColumnExtensions(client);
  }

  rxcpp::observable<std::string> getParticipantGroupsToProcess(std::shared_ptr<pep::CoreClient> client) override {
    return this->getAccessibleParticipantGroups(client);
  }

  rxcpp::observable<std::shared_ptr<std::vector<pep::PolymorphicPseudonym>>> getPpsToProcess(std::shared_ptr<pep::CoreClient> client) override {
    return rxcpp::observable<>::just(std::make_shared<std::vector<pep::PolymorphicPseudonym>>());
  }
};

class CommandFileExtensionAssign : public FileExtensionAssigningChildCommand {
public:
  explicit CommandFileExtensionAssign(CommandFileExtension& parent)
    : FileExtensionAssigningChildCommand("assign", "Assign a file extension to previously stored data", parent) {
  }

private:
  std::shared_ptr<std::vector<pep::PolymorphicPseudonym>> mPps;
  std::shared_ptr<pep::SignedTicket2> mTicket;

  rxcpp::observable<std::shared_ptr<pep::SignedTicket2>> getTicket(std::shared_ptr<pep::CoreClient> client) {
    if (mTicket != nullptr) {
      return rxcpp::observable<>::just(mTicket);
    }

    auto opts = std::make_shared<pep::requestTicket2Opts>();
    opts->modes = { "read-meta", "write-meta" };
    opts->includeAccessGroupPseudonyms = true;
    opts->columns = MultiCellQuery::GetColumns(this->getParameterValues());
    opts->columnGroups = MultiCellQuery::GetColumnGroups(this->getParameterValues());
    opts->participantGroups = MultiCellQuery::GetParticipantGroups(this->getParameterValues());

    return this->getPpsToProcess(client)
      .op(pep::RxGetOne("set of PPs"))
      .flat_map([client, opts](std::shared_ptr<std::vector<pep::PolymorphicPseudonym>> pps) {
      opts->pps = *pps;

      return client->requestTicket2(*opts);
        })
      .map([this](const pep::IndexedTicket2& ticket) {
          return this->mTicket = ticket.getTicket();
        });
  }

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return FileExtensionRequiringChildCommand::getSupportedParameters()
      + pep::commandline::Parameter("extension", "Extension to assign").alias("ext").shorthand('e').value(pep::commandline::Value<std::string>().required())
      + MultiCellQuery::Parameters();
  }

  void finalizeParameters() override {
    FileExtensionRequiringChildCommand::finalizeParameters();
    if (!pep::IsValidFileExtension(this->getParameterValues().get<std::string>("extension"))) {
      throw std::runtime_error("Please specify a valid file extension, including the leading period/dot character");
    }
  }

  rxcpp::observable<std::shared_ptr<ColumnExtensions>> getRequiredColumnExtensions(std::shared_ptr<pep::CoreClient> client) override {
    return this->getTicket(client)
      .map([this](std::shared_ptr<pep::SignedTicket2> ticket) {
      auto extension = this->getParameterValues().get<std::string>("extension");
      auto result = std::make_shared<ColumnExtensions>();
      for (const auto& column : ticket->openWithoutCheckingSignature().mColumns) {
        [[maybe_unused]] auto emplaced = result->emplace(column, extension).second;
        assert(emplaced);
      }
      return result;
        });
  }

  rxcpp::observable<std::string> getParticipantGroupsToProcess(std::shared_ptr<pep::CoreClient> client) override {
    return rxcpp::observable<>::iterate(this->getParameterValues().getOptionalMultiple<std::string>("participant-group"));
  }

  rxcpp::observable<std::shared_ptr<std::vector<pep::PolymorphicPseudonym>>> getPpsToProcess(std::shared_ptr<pep::CoreClient> client) override {
    if (mPps != nullptr) {
      return rxcpp::observable<>::just(mPps);
    }

    return MultiCellQuery::GetPps(this->getParameterValues(), client)
      .op(pep::RxToVector())
      .tap([this](std::shared_ptr<std::vector<pep::PolymorphicPseudonym>> pps) { this->mPps = pps; });
  }

};

class CommandFileExtensionList : public ChildCommandOf<CommandFileExtension> {
public:
  CommandFileExtensionList(CommandFileExtension& parent)
    : ChildCommandOf<CommandFileExtension>("list", "Lists file extensions for specified cells", parent) {
  }

private:
  struct ParticipantSpecification {
    enum Kind {
      PARTICIPANT,
      SHORT_PSEUDONYM
    };

    [[nodiscard]] static std::string KindToString(Kind kind) {
      switch (kind) {
        case PARTICIPANT: return "Participant";
        case SHORT_PSEUDONYM: return "Short pseudonym";
      }
      throw std::runtime_error("Unsupported participant specification kind: " + std::to_string(pep::ToUnderlying(kind)));
    }

    Kind kind;
    std::string value;

    auto operator<=>(const ParticipantSpecification& other) const = default;

    [[nodiscard]] std::string str() const {
      return KindToString(kind) + " " + value;
    }
  };

  using ParticipantSpecificationStringField = std::optional<std::string> ParticipantSpecification::*;
  using SpecAndPpRetrievalFunction = std::function<rxcpp::observable<MultiCellQuery::ParticipantSpecAndPp>(const pep::commandline::NamedValues&, std::shared_ptr<pep::CoreClient>)>;

  rxcpp::observable<std::shared_ptr<std::map<pep::PolymorphicPseudonym, std::set<ParticipantSpecification>>>> getParticipantSpecs(std::shared_ptr<pep::CoreClient> client, ParticipantSpecification::Kind kind, const SpecAndPpRetrievalFunction& retrieveSpecAndPp) {
    return retrieveSpecAndPp(this->getParameterValues(), client)
      .reduce(
        std::make_shared<std::map<pep::PolymorphicPseudonym, std::set<ParticipantSpecification>>>(),
        [kind](std::shared_ptr<std::map<pep::PolymorphicPseudonym, std::set<ParticipantSpecification>>> result, MultiCellQuery::ParticipantSpecAndPp specAndPp) {
          ParticipantSpecification spec{
            .kind = kind,
            .value = std::move(specAndPp.spec)
          };
          [[maybe_unused]] auto emplaced = (*result)[specAndPp.pp].emplace(std::move(spec)).second;
          assert(emplaced);
          return result;
        });
  }

  rxcpp::observable<std::shared_ptr<std::map<pep::PolymorphicPseudonym, std::set<ParticipantSpecification>>>> getParticipantSpecs(std::shared_ptr<pep::CoreClient> client) {
    return this->getParticipantSpecs(client, ParticipantSpecification::Kind::SHORT_PSEUDONYM, &MultiCellQuery::GetPpsForShortPseudonyms)
      .concat(this->getParticipantSpecs(client, ParticipantSpecification::Kind::PARTICIPANT, &MultiCellQuery::GetPpsForParticipantSpecs))
      .reduce( // Join both maps into one
        std::make_shared<std::map<pep::PolymorphicPseudonym, std::set<ParticipantSpecification>>>(),
        [](std::shared_ptr<std::map<pep::PolymorphicPseudonym, std::set<ParticipantSpecification>>> all, std::shared_ptr<std::map<pep::PolymorphicPseudonym, std::set<ParticipantSpecification>>> some) {
          for (const auto& item : *some) {
            const auto& pp = item.first;
            for (const auto& spec : item.second) {
              [[maybe_unused]] auto emplaced = (*all)[pp].emplace(spec).second;
              assert(emplaced);
            }
          }
          return all;
        });
  }

  void reportFileExtension(const std::string& participantSpec, const std::string& column, const std::map<std::string, pep::MetadataXEntry>& meta) {
    std::cout << participantSpec << ", column " << column << ": file extension ";

    auto extension = meta.find("fileExtension");
    if (extension == meta.cend()) {
      std::cout << "<none>";
    }
    else {
      std::cout << '"' << extension->second.plaintext() << '"';
    }

    std::cout << std::endl;
  }

protected:
  pep::commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf<CommandFileExtension>::getSupportedParameters()
      + MultiCellQuery::Parameters();
  }

  void finalizeParameters() override {
    const auto& parameterValues = this->getParameterValues();

    if (!MultiCellQuery::SpecifiesColumns(parameterValues)) {
      throw std::runtime_error("Query specifies no columns");
    }
    if (!MultiCellQuery::SpecifiesParticipants(parameterValues)) {
      throw std::runtime_error("Query specifies no participants");
    }

    ChildCommandOf<CommandFileExtension>::finalizeParameters();
  }

  int execute() override {
    return this->executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
      // Save (user-)specified values so we can produce output using those identifiers
      return this->getParticipantSpecs(client)
        .flat_map([this, client](std::shared_ptr<std::map<pep::PolymorphicPseudonym, std::set<ParticipantSpecification>>> specs) {
        const auto& vm = this->getParameterValues();

        pep::requestTicket2Opts opts;
        opts.modes = { "read-meta" };
        opts.includeAccessGroupPseudonyms = true;
        opts.participantGroups = MultiCellQuery::GetParticipantGroups(vm);
        opts.columnGroups = MultiCellQuery::GetColumnGroups(vm);
        opts.columns = MultiCellQuery::GetColumns(vm);

        opts.pps.reserve(specs->size());
        std::transform(specs->cbegin(), specs->cend(), std::back_inserter(opts.pps), [](const auto& pair) {return pair.first; });

        return client->requestTicket2(opts)
          .flat_map([client](pep::IndexedTicket2 indexed) {
          return client->enumerateData2(indexed.getTicket());
            })
          .map([this, specs](const std::vector<pep::EnumerateResult>& result) {
          for (const auto& entry : result) {
            auto position = specs->find(entry.mLocalPseudonyms->mPolymorphic);
            if (position != specs->cend()) { // If this participant was identified by the user on the command line, report back using that identifier
              for (auto spec : position->second) {
                this->reportFileExtension(spec.str(), entry.mColumn, entry.mMetadata.extra());
              }
            }
            else { // This PP was (only) requested as part of a participant group: we don't have a user-requested identifier for it
              assert(entry.mAccessGroupPseudonym != nullptr);
              this->reportFileExtension("Local pseudonym " + entry.mAccessGroupPseudonym->text(), entry.mColumn, entry.mMetadata.extra());
            }
          }
          return pep::FakeVoid();
            });
          });
      });
  }
};

}

std::vector<std::shared_ptr<pep::commandline::Command>> CommandFileExtension::createChildCommands() {
  std::vector<std::shared_ptr<pep::commandline::Command>> result;
  result.emplace_back(std::make_shared<CommandFileExtensionValidate>(*this));
  result.emplace_back(std::make_shared<CommandFileExtensionAssign>(*this));
  result.emplace_back(std::make_shared<CommandFileExtensionAutoAssign>(*this));
  result.emplace_back(std::make_shared<CommandFileExtensionList>(*this));
  return result;
}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandFileExtension(CliApplication& parent) {
  return std::make_shared<CommandFileExtension>(parent);
}
