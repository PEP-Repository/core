#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/application/Application.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/async/RxRequireCount.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-zip.hpp>

#include <boost/algorithm/string/split.hpp>

using namespace std::chrono;
using namespace pep::cli;

namespace {

class ParticipantState {
private:
  std::optional<std::string> mId;
  std::optional<pep::StudyContexts> mContexts;
  std::optional<bool> mIsTest;

  void readField(std::shared_ptr<pep::GlobalConfiguration> gc, const pep::EnumerateAndRetrieveResult& ear);

  void readParticipantIdentifier(std::shared_ptr<pep::GlobalConfiguration> gc, const std::string& value);
  void readStudyContexts(std::shared_ptr<pep::GlobalConfiguration> gc, const std::string& value);
  void readIsTestParticipant(std::shared_ptr<pep::GlobalConfiguration> gc, const std::string& value);

  using FieldReadMethodPtr = decltype(&ParticipantState::readParticipantIdentifier);
  using FieldReadMethods = std::unordered_map<std::string, FieldReadMethodPtr>;
  static const FieldReadMethods& GetFieldReadMethods();

public:
  static rxcpp::observable<std::shared_ptr<ParticipantState>> Get(std::shared_ptr<pep::CoreClient> client, std::shared_ptr<pep::GlobalConfiguration> gc);

  const std::optional<std::string>& getId() const noexcept { return mId; }
  const std::optional<pep::StudyContexts>& getStudyContexts() const noexcept { return mContexts; }
  bool isTestParticipant() const noexcept { return mIsTest.value_or(false); }
};

const ParticipantState::FieldReadMethods& ParticipantState::GetFieldReadMethods() {
  const static FieldReadMethods instance{{"ParticipantIdentifier", &ParticipantState::readParticipantIdentifier},
                                         {"StudyContexts", &ParticipantState::readStudyContexts},
                                         {"IsTestParticipant", &ParticipantState::readIsTestParticipant}};
  return instance;
}

void ParticipantState::readField(std::shared_ptr<pep::GlobalConfiguration> gc, const pep::EnumerateAndRetrieveResult& ear) {
  const auto& readers = GetFieldReadMethods();
  auto entry = readers.find(ear.mColumn);
  if (entry == readers.cend()) {
    throw std::runtime_error("Cannot read participant state from field " + ear.mColumn);
  }
  auto method = entry->second;

  assert(ear.mDataSet); // TODO: support data that isn't returned inline
  (this->*method)(gc, ear.mData);
}

void ParticipantState::readParticipantIdentifier(std::shared_ptr<pep::GlobalConfiguration> gc, const std::string& value) {
  assert(mId == std::nullopt);
  mId = value;
}

void ParticipantState::readStudyContexts(std::shared_ptr<pep::GlobalConfiguration> gc, const std::string& value) {
  assert(mContexts == std::nullopt);
  mContexts = gc->getStudyContexts().parse(value);
}

void ParticipantState::readIsTestParticipant(std::shared_ptr<pep::GlobalConfiguration> gc, const std::string& value) {
  assert(mIsTest == std::nullopt);
  mIsTest = pep::StringToBool(value);
}

rxcpp::observable<std::shared_ptr<ParticipantState>> ParticipantState::Get(std::shared_ptr<pep::CoreClient> client, std::shared_ptr<pep::GlobalConfiguration> gc) {
  using StateMap = std::unordered_map<uint32_t, std::shared_ptr<ParticipantState>>;

  pep::enumerateAndRetrieveData2Opts opts;
  opts.groups.push_back("*");
  for (const auto& entry : GetFieldReadMethods()) { // Read all columns that we can process
    opts.columns.push_back(entry.first);
  }

  return client->enumerateAndRetrieveData2(opts) // Get salient data for every row
    .reduce(
      std::make_shared<StateMap>(),
      [gc](std::shared_ptr<StateMap> states, const pep::EnumerateAndRetrieveResult& ear) {
        auto& participant = (*states)[ear.mLocalPseudonymsIndex];
        if (participant == nullptr) {
          participant = std::make_shared<ParticipantState>();
        }
        participant->readField(gc, ear);
        return states;
      })
    .concat_map([](std::shared_ptr<StateMap> states) {return pep::RxIterate(std::move(*states)); })
    .map([](std::pair<const uint32_t, std::shared_ptr<ParticipantState>> pair) {return std::move(pair.second); })
    .filter([](std::shared_ptr<ParticipantState> participant) {return participant->mId != std::nullopt; }); // Rows without ParticipantIdentifier cannot be processed
}

class ParticipantGroup : public pep::SharedConstructor<ParticipantGroup> {
  friend class pep::SharedConstructor<ParticipantGroup>;

public:
  using Map = std::map<std::string, std::shared_ptr<ParticipantGroup>>;

  class AutoAssignContext : public pep::SharedConstructor<AutoAssignContext> {
    friend class pep::SharedConstructor<AutoAssignContext>;

  private:
    std::shared_ptr<pep::CoreClient> mClient;
    bool mApply;
    std::map<std::string, std::string> mMappings;

    static inline std::string GetGroupNamePrefix() { return "all"; }
    static inline std::string GetContextBoundGroupNameDelimiter() { return "-"; }
    static inline std::string GetContextBoundGroupNamePrefix() { return GetGroupNamePrefix() + GetContextBoundGroupNameDelimiter(); }

    AutoAssignContext(std::shared_ptr<pep::CoreClient> client, bool apply, const std::vector<std::string>& mappings);
    static void ToLower(std::string& value);

  public:
    std::string getGroupNameForStudyContext(const std::optional<pep::StudyContext>& context = std::nullopt) const;
    inline std::shared_ptr<pep::CoreClient> getClient() const noexcept { return mClient; }
    inline bool applyUpdates() const noexcept { return mApply; }
    static bool IsAutoAssignedGroupName(const std::string& name);
    static void OnManualAssignment(const std::string& group);
  };

private:
  std::string mName;
  std::set<std::string> mParticipantIds;

  explicit ParticipantGroup(const std::string& name) : mName(name) {}

  static void IncludeParticipant(Map& destination, const std::string& group, const std::string& participantId);
  static void IncludeRequiredAssignments(Map& destination, const ParticipantState& participant, const pep::GlobalConfiguration& gc, const AutoAssignContext& context);

  static rxcpp::observable<pep::FakeVoid> UpdateGroupContents(std::shared_ptr<AutoAssignContext> context, const std::string& name, const std::set<std::string>& required, const std::set<std::string>& existing);
  static rxcpp::observable<pep::FakeVoid> UpdateGroupConfiguration(std::shared_ptr<AutoAssignContext> context, std::shared_ptr<ParticipantGroup> required, std::shared_ptr<ParticipantGroup> existing);
  static rxcpp::observable<pep::FakeVoid> UpdateGroupConfigurations(std::shared_ptr<AutoAssignContext> context, const Map& required, const Map& existing);
  static rxcpp::observable<std::shared_ptr<Map>> GetRequired(std::shared_ptr<AutoAssignContext> context);
  static rxcpp::observable<std::shared_ptr<Map>> GetExisting(std::shared_ptr<pep::CoreClient> client);

public:
  static rxcpp::observable<pep::FakeVoid> AutoAssign(std::shared_ptr<AutoAssignContext> context);
};

void ParticipantGroup::IncludeParticipant(Map& destination, const std::string& group, const std::string& participantId) {
  Map::iterator position = destination.find(group);
  if (position == destination.end()) {
    auto inserted = destination.emplace(group, ParticipantGroup::Create(group));
    assert(inserted.second);
    position = inserted.first;
  }
  assert(position != destination.end());
  position->second->mParticipantIds.emplace(participantId);
}

void ParticipantGroup::IncludeRequiredAssignments(Map& destination, const ParticipantState& participant, const pep::GlobalConfiguration& gc, const AutoAssignContext& context) {
  if (!participant.isTestParticipant()) {
    const auto& participantId = *participant.getId();
    IncludeParticipant(destination, context.getGroupNameForStudyContext(std::nullopt), participantId);

    if (participant.getStudyContexts() != std::nullopt) {
      for (const auto& sc : participant.getStudyContexts()->getItems()) {
        IncludeParticipant(destination, context.getGroupNameForStudyContext(sc), participantId);
      }
    }
    else {
      auto defaultContext = gc.getStudyContexts().getDefault();
      if (defaultContext != nullptr) {
        IncludeParticipant(destination, context.getGroupNameForStudyContext(*defaultContext), participantId);
      }
    }
  }
}

rxcpp::observable<std::shared_ptr<ParticipantGroup::Map>> ParticipantGroup::GetRequired(std::shared_ptr<AutoAssignContext> context) {
  return context->getClient()->getGlobalConfiguration()
    .concat_map([context](std::shared_ptr<pep::GlobalConfiguration> gc) {
    return ParticipantState::Get(context->getClient(), gc)
      .reduce(
        std::make_shared<Map>(),
        [gc, context](std::shared_ptr<Map> result, std::shared_ptr<ParticipantState> participant) {
          IncludeRequiredAssignments(*result, *participant, *gc, *context);
          return result;
        }
      );
      });
}

rxcpp::observable<std::shared_ptr<ParticipantGroup::Map>> ParticipantGroup::GetExisting(std::shared_ptr<pep::CoreClient> client) {
  return client->getAccessManagerProxy()->amaQuery(pep::AmaQuery{})
    .concat_map([](const pep::AmaQueryResponse& response) {return RxIterate(response.mParticipantGroups); })
    .filter([](const pep::AmaQRParticipantGroup& group) {return AutoAssignContext::IsAutoAssignedGroupName(group.mName); })
    .concat_map([client](const pep::AmaQRParticipantGroup& group) {
    pep::enumerateAndRetrieveData2Opts opts;
    opts.groups.push_back(group.mName);
    opts.columns.push_back("ParticipantIdentifier");
    // TODO: re-use ticket from ParticipantGroup::GetRequired
    return client->enumerateAndRetrieveData2(opts)
      .reduce(
        ParticipantGroup::Create(group.mName),
        [](std::shared_ptr<ParticipantGroup> group, const pep::EnumerateAndRetrieveResult& ear) {
          assert(ear.mDataSet); // TODO: support data not being returned inline
          assert(ear.mColumn == "ParticipantIdentifier");
          group->mParticipantIds.emplace(ear.mData);
          return group;
        }
      );
      })
    .as_dynamic() // Reduce compiler memory usage
    .reduce( // TODO: add RxToMap utility function
      std::make_shared<Map>(),
      [](std::shared_ptr<Map> result, std::shared_ptr<ParticipantGroup> group) {
        [[maybe_unused]] auto emplaced = result->emplace(group->mName, group);
        assert(emplaced.second);
        return result;
      })
    .op(pep::RxGetOne("existing participant group collection"));
}

rxcpp::observable<pep::FakeVoid> ParticipantGroup::UpdateGroupContents(std::shared_ptr<AutoAssignContext> context, const std::string& name, const std::set<std::string>& required, const std::set<std::string>& existing) {
  struct Inclusion {
    bool required = false;
    bool existing = false;
  };
  std::map<std::string, Inclusion> participants;
  for (const auto& participant: required) {
    participants[participant].required = true;
  }
  for (const auto& participant : existing) {
    participants[participant].existing = true;
  }

  return pep::RxIterate(std::move(participants))
    .filter([](const auto& kvp) {
    assert(kvp.second.required || kvp.second.existing);
    return kvp.second.required != kvp.second.existing;
      })
    .concat_map([context, group = pep::MakeSharedCopy(name)](const auto& kvp) {
    auto traits = pep::MakeSharedCopy(kvp);
    return context->getClient()->parsePPorIdentity(traits->first)
      .concat_map([context, group, traits](const pep::PolymorphicPseudonym& pp) -> rxcpp::observable<pep::FakeVoid> {
      if (traits->second.required) {
        std::cout << "Adding " << traits->first << " to group " << *group << std::endl;
        if (!context->applyUpdates()) {
          return rxcpp::observable<>::just(pep::FakeVoid());
        }
        return context->getClient()->getAccessManagerProxy()->amaAddParticipantToGroup(*group, pp);
      }
      assert(traits->second.existing);
      std::cout << "Removing " << traits->first << " from group " << *group << std::endl;
      if (!context->applyUpdates()) {
        return rxcpp::observable<>::just(pep::FakeVoid());
      }
      return context->getClient()->getAccessManagerProxy()->amaRemoveParticipantFromGroup(*group, pp);
    });
  });
}

rxcpp::observable<pep::FakeVoid> ParticipantGroup::UpdateGroupConfiguration(std::shared_ptr<AutoAssignContext> context, std::shared_ptr<ParticipantGroup> required, std::shared_ptr<ParticipantGroup> existing) {
  assert(required != nullptr || existing != nullptr);

  if (existing == nullptr) {
    std::cout << "Creating group " << required->mName << std::endl;
    rxcpp::observable<pep::FakeVoid> create;
    if (context->applyUpdates()) {
      create = context->getClient()->getAccessManagerProxy()->amaCreateParticipantGroup(required->mName);
    }
    else {
      create = rxcpp::observable<>::just(pep::FakeVoid());
    }

    return create
      .concat_map([context, required](const pep::FakeVoid&) {return UpdateGroupContents(context, required->mName, required->mParticipantIds, std::set<std::string>()); });
  }

  if (required == nullptr) {
    return UpdateGroupContents(context, existing->mName, std::set<std::string>(), existing->mParticipantIds)
      .op(RxInstead(pep::FakeVoid())) // Ensure remainder of pipeline gets exactly one item
      .concat_map([context, existing](const pep::FakeVoid&) -> rxcpp::observable<pep::FakeVoid> {
      std::cout << "Removing group " << existing->mName << std::endl;
      if (!context->applyUpdates()) {
        return rxcpp::observable<>::just(pep::FakeVoid());
      }
      return context->getClient()->getAccessManagerProxy()->amaRemoveParticipantGroup(existing->mName, false);
        });
  }

  return UpdateGroupContents(context, required->mName, required->mParticipantIds, existing->mParticipantIds);
}

rxcpp::observable<pep::FakeVoid> ParticipantGroup::UpdateGroupConfigurations(std::shared_ptr<AutoAssignContext> context, const Map& required, const Map& existing) {
  std::map<std::string, std::pair<std::shared_ptr<ParticipantGroup>, std::shared_ptr<ParticipantGroup>>> pairs;
  for (const auto& kvp : required) {
    pairs[kvp.first].first = kvp.second;
  }
  for (const auto& kvp : existing) {
    pairs[kvp.first].second = kvp.second;
  }

  return RxIterate(std::move(pairs))
    .map([](const auto& kvp) {return kvp.second; })
    .concat_map([context](const auto& pair) {return UpdateGroupConfiguration(context, pair.first, pair.second); });
}

rxcpp::observable<pep::FakeVoid> ParticipantGroup::AutoAssign(std::shared_ptr<AutoAssignContext> context) {
  return GetRequired(context)
    .zip(GetExisting(context->getClient()))
    .concat_map([context](const auto& tuple) { return ParticipantGroup::UpdateGroupConfigurations(context, *std::get<0>(tuple), *std::get<1>(tuple)); });
}

ParticipantGroup::AutoAssignContext::AutoAssignContext(std::shared_ptr<pep::CoreClient> client, bool apply, const std::vector<std::string>& mappings)
  : mClient(client), mApply(apply) {
  for (const auto& mapping : mappings) {
    std::vector<std::string> parts;
    boost::split(parts, mapping, boost::is_any_of("="));
    if (parts.size() != 2) {
      throw std::runtime_error("Name mapping must have form \"original=replacement\"");
    }
    ToLower(parts[0]);
    ToLower(parts[1]);
    auto emplaced = mMappings.emplace(parts[0], parts[1]);
    if (!emplaced.second) {
      throw std::runtime_error("Multiple name mappings specified for original " + parts[0]);
    }
  }
}

void ParticipantGroup::AutoAssignContext::ToLower(std::string& value) {
  std::transform(value.begin(), value.end(), value.begin(), [](char c) {return std::tolower(c); });
}

std::string ParticipantGroup::AutoAssignContext::getGroupNameForStudyContext(const std::optional<pep::StudyContext>& context) const {
  std::string result = GetGroupNamePrefix();

  if (context != std::nullopt) {
    // Get and canonicalize context ID
    auto id = context->getId();
    ToLower(id);

    // Apply mapping if there is one
    auto mapping = mMappings.find(id);
    if (mapping != mMappings.cend()) {
      id = mapping->second;
    }

    // Append (possibly mapped) context ID to participant group name
    result += GetContextBoundGroupNameDelimiter() + id;
  }

  assert(IsAutoAssignedGroupName(result));
  return result;
}

bool ParticipantGroup::AutoAssignContext::IsAutoAssignedGroupName(const std::string& name) {
  return name == GetGroupNamePrefix() || name.starts_with(GetContextBoundGroupNamePrefix());
}

void ParticipantGroup::AutoAssignContext::OnManualAssignment(const std::string& group) {
  if (IsAutoAssignedGroupName(group)) {
    std::cout.flush();
    std::cerr << "Manual configuration of participant group '" << group << "' will be discarded if/when automatic group assignment is applied." << std::endl;
  }
}


class CommandAma : public ChildCommandOf<CliApplication> {

 public:
  explicit CommandAma(CliApplication& parent)
    : ChildCommandOf<CliApplication>("ama", "Administer access manager", parent) {
  }

private:
  class CommandAmaQuery : public ChildCommandOf<CommandAma> {
  public:
    explicit CommandAmaQuery(CommandAma& parent)
      : ChildCommandOf<CommandAma>("query", "Query state (column, rules, etc.)", parent) {
    }

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#ama-query"; }

    pep::commandline::Parameters getSupportedParameters() const override {
      return ChildCommandOf<CommandAma>::getSupportedParameters()
        + pep::commandline::Parameter("script-print", "Prints specified type of data without pretty printing").value(pep::commandline::Value<std::string>()
          .allow(std::vector<std::string>({"columns", "column-groups", "column-group-access-rules", "participant-groups", "participant-group-access-rules" })))
        + pep::commandline::Parameter("at", "Query for this timestamp (milliseconds since 1970-01-01 00:00:00 in UTC), defaults to now if omitted")
            .value(pep::commandline::Value<milliseconds::rep>())
        + pep::commandline::Parameter("column", "Match these columns").value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"))
        + pep::commandline::Parameter("column-group", "Match these column groups").value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"))
        + pep::commandline::Parameter("user-group", "Match these user groups").value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"))
        + pep::commandline::Parameter("participant-group", "Match these participant groups").value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"))
        + pep::commandline::Parameter("column-mode", "Match these column-modes").value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"))
        + pep::commandline::Parameter("participant-group-mode", "Match these participant-group-modes").value(pep::commandline::Value<std::string>().defaultsTo("", "empty string"));
    }

    int execute() override {
      const auto& vm = this->getParameterValues();
      std::optional<std::string> scriptPrintFilter;
      if(vm.has("script-print")) {
        scriptPrintFilter = vm.get<std::string>("script-print");
      }

      return executeEventLoopFor([&vm, scriptPrintFilter](std::shared_ptr<pep::CoreClient> client) {
        pep::AmaQuery query{
          .mAt = pep::GetOptionalValue(vm.getOptional<milliseconds::rep>("at"), [](milliseconds::rep ms) {
            return pep::Timestamp(milliseconds{ms});
          }),
          .mColumnFilter = vm.get<std::string>("column"),
          .mColumnGroupFilter = vm.get<std::string>("column-group"),
          .mParticipantGroupFilter = vm.get<std::string>("participant-group"),
          .mUserGroupFilter = vm.get<std::string>("user-group"),
          .mColumnGroupModeFilter = vm.get<std::string>("column-mode"),
          .mParticipantGroupModeFilter = vm.get<std::string>("participant-group-mode"),
        };
        return client->getAccessManagerProxy()->amaQuery(std::move(query))
        .map([scriptPrintFilter](pep::AmaQueryResponse res) {

          bool prettyPrint = !scriptPrintFilter.has_value();
          std::string offset = prettyPrint ? "  " : "";

          if(!scriptPrintFilter.has_value() || scriptPrintFilter == "columns") {
            std::sort(res.mColumns.begin(), res.mColumns.end(), [](auto& a, auto& b) {return a.mName < b.mName; });
            if(prettyPrint)
              std::cout << "Columns (" << res.mColumns.size() << "):" << std::endl;
            for (auto &col : res.mColumns)
              std::cout << offset << col.mName << std::endl;
            std::cout << std::endl;
          }

          if(!scriptPrintFilter.has_value() || scriptPrintFilter == "column-groups") {
            if (prettyPrint)
              std::cout << "ColumnGroups (" << res.mColumnGroups.size() << "):" << std::endl;
            std::sort(res.mColumnGroups.begin(), res.mColumnGroups.end(),
                      [](auto &a, auto &b) { return a.mName < b.mName; });
            for (auto &cg : res.mColumnGroups) {
              std::cout << offset << cg.mName << " (" << cg.mColumns.size() << ")" << std::endl;
              std::sort(cg.mColumns.begin(), cg.mColumns.end());
              for (auto &col : cg.mColumns)
                std::cout << offset << "  " << col << std::endl;
              std::cout << std::endl;
            }
            std::cout << std::endl;
          }

          if(!scriptPrintFilter.has_value() || scriptPrintFilter == "column-group-access-rules") {
            std::sort(
              res.mColumnGroupAccessRules.begin(),
              res.mColumnGroupAccessRules.end(),
              [](auto &a, auto &b) {
                return std::make_tuple(a.mAccessGroup, a.mColumnGroup, a.mMode)
                       < std::make_tuple(b.mAccessGroup, b.mColumnGroup, b.mMode);
              });
            if (prettyPrint)
              std::cout << "ColumnGroupAccessRules (" << res.mColumnGroupAccessRules.size() << "):" << std::endl;
            for (auto &cgar : res.mColumnGroupAccessRules)
              std::cout << offset
                        << std::left << std::setw(30) << cgar.mColumnGroup << " "
                        << std::setw(30) << cgar.mAccessGroup << " "
                        << std::setw(10) << cgar.mMode << std::endl;
            std::cout << std::endl;
          }

          if (!scriptPrintFilter.has_value() || scriptPrintFilter == "participant-groups") {
            std::sort(res.mParticipantGroups.begin(), res.mParticipantGroups.end(), [](auto &a, auto &b) { return a.mName < b.mName; });
            if (prettyPrint)
              std::cout << "ParticipantGroups (" << res.mParticipantGroups.size() << "):" << std::endl;
            for (auto &group : res.mParticipantGroups)
              std::cout << offset << group.mName << std::endl;
            std::cout << std::endl;
          }

          if (!scriptPrintFilter.has_value() || scriptPrintFilter == "participant-group-access-rules") {
            std::sort(
              res.mParticipantGroupAccessRules.begin(),
              res.mParticipantGroupAccessRules.end(),
              [](auto &a, auto &b) {
                return std::make_tuple(a.mUserGroup, a.mParticipantGroup, a.mMode)
                       < std::make_tuple(b.mUserGroup, b.mParticipantGroup, b.mMode);
              });
            if(prettyPrint)
              std::cout << "ParticipantGroupAccessRules (" << res.mParticipantGroupAccessRules.size() << "):" << std::endl;
            for (auto &cgar : res.mParticipantGroupAccessRules)
              std::cout << offset
                        << std::left << std::setw(30) << cgar.mParticipantGroup << " "
                        << std::setw(30) << cgar.mUserGroup << " "
                        << std::setw(10) << cgar.mMode << std::endl;

            std::cout << std::endl;
            if (prettyPrint) {
              std::cerr
                << "The \"read\" access privilege grants access to \"read-meta\" data as well." << '\n'
                << "The \"write-meta\" access privilege grants access to \"write\" data as well." << '\n'
                << pep::UserGroup::DataAdministrator << " has implicit full access to all participant groups." << '\n'
                << pep::UserGroup::DataAdministrator << " has implicit \"read-meta\" access to all column groups." << std::endl;
            }
          }

          return pep::FakeVoid();
        });
      });
    }
  };

  class CommandAmaCgar : public ChildCommandOf<CommandAma> {
  public:
    explicit CommandAmaCgar(CommandAma& parent)
      : ChildCommandOf<CommandAma>("cgar", "Administer column group access rules", parent) {
    }

  private:
    class AmaCgarSubCommand : public ChildCommandOf<CommandAmaCgar> {
    public:
      using AmProxyMethod = rxcpp::observable<pep::FakeVoid> (pep::AccessManagerProxy::*)(std::string, std::string, std::string) const;

    private:
      AmProxyMethod mMethod;

    public:
      AmaCgarSubCommand(const std::string& name, const std::string& description, AmProxyMethod method, CommandAmaCgar& parent)
        : ChildCommandOf<CommandAmaCgar>(name, description, parent), mMethod(method) {
      }

    protected:
      pep::commandline::Parameters getSupportedParameters() const override {
        return ChildCommandOf<CommandAmaCgar>::getSupportedParameters()
          + pep::commandline::Parameter("column-group", "Name of columnGroup").value(pep::commandline::Value<std::string>().positional().required())
          + pep::commandline::Parameter("access-group", "Name of accessGroup").value(pep::commandline::Value<std::string>().positional().required())
          + pep::commandline::Parameter("mode", "Access mode").value(pep::commandline::Value<std::string>().positional().required().allow(std::vector<std::string>({ "read-meta", "write-meta", "read", "write" })));
      }

      int execute() override {
        const auto& vm = this->getParameterValues();
        return executeEventLoopFor([&vm, method = mMethod](std::shared_ptr<pep::CoreClient> client) {
          auto& am = *client->getAccessManagerProxy();
          return (am.*method)(
            vm.get<std::string>("column-group"),
            vm.get<std::string>("access-group"),
            vm.get<std::string>("mode"));
          });
      }
    };

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#ama-cgar"; }

    std::vector<std::shared_ptr<Command>> createChildCommands() override {
      return {std::make_shared<AmaCgarSubCommand>("create",
                                                  "Creates a new column-group-access-rule",
                                                  &pep::AccessManagerProxy::amaCreateColumnGroupAccessRule,
                                                  *this),
              std::make_shared<AmaCgarSubCommand>("remove",
                                                  "Remove a column-group-access-rule",
                                                  &pep::AccessManagerProxy::amaRemoveColumnGroupAccessRule,
                                                  *this)};
    }
  };

  class CommandAmaPgar : public ChildCommandOf<CommandAma> {
  public:
    explicit CommandAmaPgar(CommandAma& parent)
      : ChildCommandOf<CommandAma>("pgar", "Administer participant group access rules", parent) {
    }

  private:
    class AmaPgarSubCommand : public ChildCommandOf<CommandAmaPgar> {
    public:
      using AmProxyMethod = rxcpp::observable<pep::FakeVoid> (pep::AccessManagerProxy::*)(std::string, std::string, std::string) const;

    private:
      AmProxyMethod mMethod;

    public:
      AmaPgarSubCommand(const std::string& name, const std::string& description, AmProxyMethod method, CommandAmaPgar& parent)
        : ChildCommandOf<CommandAmaPgar>(name, description, parent), mMethod(method) {
      }

    protected:
      pep::commandline::Parameters getSupportedParameters() const override {
        return ChildCommandOf<CommandAmaPgar>::getSupportedParameters()
          + pep::commandline::Parameter("group", "Name of (participant) group").value(pep::commandline::Value<std::string>().positional().required())
          + pep::commandline::Parameter("access-group", "Name of accessGroup").value(pep::commandline::Value<std::string>().positional().required())
          + pep::commandline::Parameter("mode", "Access mode").value(pep::commandline::Value<std::string>().positional().required().allow(std::vector<std::string>({ "enumerate", "access" })));
      }

      int execute() override {
        const auto& vm = this->getParameterValues();
        return executeEventLoopFor([&vm, method = mMethod](std::shared_ptr<pep::CoreClient> client) {
          auto& am = *client->getAccessManagerProxy();
          return (am.*method)(
            vm.get<std::string>("group"),
            vm.get<std::string>("access-group"),
            vm.get<std::string>("mode"));
          });
      }
    };

  protected:
    std::vector<std::shared_ptr<Command>> createChildCommands() override {
      return {std::make_shared<AmaPgarSubCommand>("create",
                                                  "Creates a (participant) group-access-rule",
                                                  &pep::AccessManagerProxy::amaCreateGroupAccessRule,
                                                  *this),
              std::make_shared<AmaPgarSubCommand>("remove",
                                                  "Remove a (participant) group-access-rule",
                                                  &pep::AccessManagerProxy::amaRemoveGroupAccessRule,
                                                  *this)};
    }
  };

  class CommandAmaColumn : public ChildCommandOf<CommandAma> {
  public:
    explicit CommandAmaColumn(CommandAma& parent)
      : ChildCommandOf<CommandAma>("column", "Administer columns", parent) {
    }

  private:
    class AmaColumnSubCommand : public ChildCommandOf<CommandAmaColumn> {
    protected:
      AmaColumnSubCommand(const std::string& name, const std::string& description, CommandAmaColumn& parent)
        : ChildCommandOf<CommandAmaColumn>(name, description, parent) {
      }

    protected:
      pep::commandline::Parameters getSupportedParameters() const override {
        return ChildCommandOf<CommandAmaColumn>::getSupportedParameters()
          + pep::commandline::Parameter("column", "Name of column").value(pep::commandline::Value<std::string>().positional().required());
      }

      std::string getSpecifiedColumnName() const {
        return this->getParameterValues().get<std::string>("column");
      }
    };

    class AmaColumnExistenceSubCommand : public AmaColumnSubCommand {
    public:
      using AmProxyMethod = rxcpp::observable<pep::FakeVoid> (pep::AccessManagerProxy::*)(std::string) const;

    private:
      AmProxyMethod mMethod;

    public:
      AmaColumnExistenceSubCommand(const std::string& name, const std::string& description, AmProxyMethod method, CommandAmaColumn& parent)
        : AmaColumnSubCommand(name, description, parent), mMethod(method) {
      }

    protected:
      int execute() override {
        return executeEventLoopFor([column = this->getSpecifiedColumnName(), method = mMethod](std::shared_ptr<pep::CoreClient> client) {
          auto& am = *client->getAccessManagerProxy();
          return (am.*method)(column);
        });
      }
    };

    class AmaColumnGroupingSubCommand : public AmaColumnSubCommand {
    public:
      using AmProxyMethod = rxcpp::observable<pep::FakeVoid> (pep::AccessManagerProxy::*)(std::string, std::string) const;

    private:
      AmProxyMethod mMethod;

    public:
      AmaColumnGroupingSubCommand(const std::string& name, const std::string& description, AmProxyMethod method, CommandAmaColumn& parent)
        : AmaColumnSubCommand(name, description, parent), mMethod(method) {
      }

    protected:
      pep::commandline::Parameters getSupportedParameters() const override {
        return AmaColumnSubCommand::getSupportedParameters()
          + pep::commandline::Parameter("group", "Name of column group").value(pep::commandline::Value<std::string>().positional().required());
      }

      int execute() override {
        auto column = this->getSpecifiedColumnName();
        auto group = this->getParameterValues().get<std::string>("group");
        return executeEventLoopFor([column, group, method = mMethod](std::shared_ptr<pep::CoreClient> client) {
          auto& am = *client->getAccessManagerProxy();
          return (am.*method)(column, group);
        });
      }
    };

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#ama-column"; }

    std::vector<std::shared_ptr<Command>> createChildCommands() override {
      return {std::make_shared<AmaColumnExistenceSubCommand>("create",
                                                             "Create new column",
                                                             &pep::AccessManagerProxy::amaCreateColumn,
                                                             *this),
              std::make_shared<AmaColumnExistenceSubCommand>("remove",
                                                             "Remove column",
                                                             &pep::AccessManagerProxy::amaRemoveColumn,
                                                             *this),
              std::make_shared<AmaColumnGroupingSubCommand>("addTo",
                                                            "Add column to group",
                                                            &pep::AccessManagerProxy::amaAddColumnToGroup,
                                                            *this),
              std::make_shared<AmaColumnGroupingSubCommand>("removeFrom",
                                                            "Remove column from group",
                                                            &pep::AccessManagerProxy::amaRemoveColumnFromGroup,
                                                            *this)};
    }
  };

  class CommandAmaColumnGroup : public ChildCommandOf<CommandAma> {
  public:
    explicit CommandAmaColumnGroup(CommandAma& parent)
      : ChildCommandOf<CommandAma>("columnGroup", "Administer column groups", parent) {
    }

  private:


    class AmaColumnGroupCreateCommand : public ChildCommandOf<CommandAmaColumnGroup> {
    public:
      AmaColumnGroupCreateCommand(CommandAmaColumnGroup& parent)
        : ChildCommandOf<CommandAmaColumnGroup>("create", "Create new column group", parent) {}

    protected:
      pep::commandline::Parameters getSupportedParameters() const override {
        return ChildCommandOf<CommandAmaColumnGroup>::getSupportedParameters()
          + pep::commandline::Parameter("name", "Name of column group").value(pep::commandline::Value<std::string>().positional().required());
      }

      int execute() override {
        const auto& vm = this->getParameterValues();
        return executeEventLoopFor([column = vm.get<std::string>("name")](std::shared_ptr<pep::CoreClient> client) {
          return client->getAccessManagerProxy()->amaCreateColumnGroup(column);
        });
      }
    };

    class AmaColumnGroupRemoveCommand : public ChildCommandOf<CommandAmaColumnGroup> {
    public:
      AmaColumnGroupRemoveCommand(CommandAmaColumnGroup& parent)
        : ChildCommandOf<CommandAmaColumnGroup>("remove", "Remove column group", parent) {}

    protected:
      pep::commandline::Parameters getSupportedParameters() const override {
        return ChildCommandOf<CommandAmaColumnGroup>::getSupportedParameters()
          + pep::commandline::Parameter("name", "Name of column group").value(pep::commandline::Value<std::string>().positional().required())
          + pep::commandline::Parameter("force", "Remove column group even if it still has associated columns and / or access rules. Also removes all column connections and access rules.").shorthand('f');
      }

      int execute() override {
        const auto& vm = this->getParameterValues();
        return executeEventLoopFor([column = vm.get<std::string>("name"), force = vm.has("force")](std::shared_ptr<pep::CoreClient> client) {
          return client->getAccessManagerProxy()->amaRemoveColumnGroup(column, force);
        });
      }
    };

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#ama-columngroup"; }

    std::vector<std::shared_ptr<Command>> createChildCommands() override {
      return {std::make_shared<AmaColumnGroupCreateCommand>(*this),
              std::make_shared<AmaColumnGroupRemoveCommand>(*this)};
    }
  };

  class CommandAmaParticipantGroup : public ChildCommandOf<CommandAma> {
  public:
    explicit CommandAmaParticipantGroup(CommandAma& parent)
      : ChildCommandOf<CommandAma>("group", "Administer participant groups", parent) {
    }

  private:
    class AmaParticipantGroupSubCommand : public ChildCommandOf<CommandAmaParticipantGroup> {
    protected:
      AmaParticipantGroupSubCommand(const std::string& name, const std::string& description, CommandAmaParticipantGroup& parent)
        : ChildCommandOf<CommandAmaParticipantGroup>(name, description, parent) {
      }

      pep::commandline::Parameters getSupportedParameters() const override {
        return ChildCommandOf<CommandAmaParticipantGroup>::getSupportedParameters()
          + pep::commandline::Parameter("group", "Name of participant group").value(pep::commandline::Value<std::string>().positional().required());
      }

      std::string getParticipantGroupName() const {
        return this->getParameterValues().get<std::string>("group");
      }
    };

    class AmaParticipantGroupCreateCommand : public AmaParticipantGroupSubCommand {
    public:
      AmaParticipantGroupCreateCommand(CommandAmaParticipantGroup& parent)
        : AmaParticipantGroupSubCommand("create", "Create new participant group", parent) {
      }

    protected:
      int execute() override {
        return executeEventLoopFor([group = this->getParticipantGroupName()](std::shared_ptr<pep::CoreClient> client) {
          return client->getAccessManagerProxy()->amaCreateParticipantGroup(group);
        });
      }
    };

    class AmaParticipantGroupRemoveSubCommand : public AmaParticipantGroupSubCommand {
    public:
      AmaParticipantGroupRemoveSubCommand(CommandAmaParticipantGroup& parent)
        : AmaParticipantGroupSubCommand("remove", "Remove participant group", parent) {
      }

    protected:
      pep::commandline::Parameters getSupportedParameters() const override {
        return AmaParticipantGroupSubCommand::getSupportedParameters()
          + pep::commandline::Parameter("force", "Remove participant group even if it still has associated participants and / or access rules. Also removes all participant connections and access rules.").shorthand('f');
      }

      int execute() override {
        return executeEventLoopFor([group = this->getParticipantGroupName(), force = this->getParameterValues().has("force")](std::shared_ptr<pep::CoreClient> client) {
          return client->getAccessManagerProxy()->amaRemoveParticipantGroup(group, force);
        });
      }
    };

    class AmaParticipantGroupClearSubCommand : public AmaParticipantGroupSubCommand {
    public:
      AmaParticipantGroupClearSubCommand(CommandAmaParticipantGroup& parent)
        : AmaParticipantGroupSubCommand("clear", "Remove all participants from group", parent) {}

    protected:
      int execute() override {
        return executeEventLoopFor([group = this->getParticipantGroupName()](std::shared_ptr<pep::CoreClient> client) {
          pep::requestTicket2Opts requestTicketOpts;
          requestTicketOpts.participantGroups.emplace_back(group);
          requestTicketOpts.modes.emplace_back("read-meta");
          return client->requestTicket2(requestTicketOpts)
            .flat_map([group, client](const pep::IndexedTicket2& indexed) {
            auto ticket = indexed.openTicketWithoutCheckingSignature();
            std::vector<pep::PolymorphicPseudonym> pps;
            pps.reserve(ticket->mPseudonyms.size());
            std::transform(ticket->mPseudonyms.begin(), ticket->mPseudonyms.end(), std::back_inserter(pps), [](const pep::LocalPseudonyms& local) {return local.mPolymorphic; });
            return client->getAccessManagerProxy()->amaRemoveParticipantsFromGroup(group, pps);
              });
          });
      }
    };


    class AmaParticipantGroupingSubCommand : public AmaParticipantGroupSubCommand {
    public:
      using AmProxyMethod = rxcpp::observable<pep::FakeVoid> (pep::AccessManagerProxy::*)(std::string, const pep::PolymorphicPseudonym&) const;

    private:
      AmProxyMethod mMethod;

    public:
      AmaParticipantGroupingSubCommand(const std::string& name, const std::string& description, AmProxyMethod method, CommandAmaParticipantGroup& parent)
        : AmaParticipantGroupSubCommand(name, description, parent), mMethod(method) {
      }

    protected:
      pep::commandline::Parameters getSupportedParameters() const override {
        return AmaParticipantGroupSubCommand::getSupportedParameters()
          + pep::commandline::Parameter("participant", "Participant identifier or polymorphic pseudonym").value(pep::commandline::Value<std::string>().positional().required());
      }

      int execute() override {
        return executeEventLoopFor([this](std::shared_ptr<pep::CoreClient> client) {
          return client->parsePPorIdentity(this->getParameterValues().get<std::string>("participant"))
            .concat_map([client, this](const pep::PolymorphicPseudonym& pp) {
            auto group = this->getParticipantGroupName();
            ParticipantGroup::AutoAssignContext::OnManualAssignment(group);
            auto& am = *client->getAccessManagerProxy();
            return (am.*mMethod)(group, pp);
              });
          });
      }
    };

    class AmaParticipantGroupAutoAssignCommand : public ChildCommandOf<CommandAmaParticipantGroup> {
    public:
      explicit AmaParticipantGroupAutoAssignCommand(CommandAmaParticipantGroup& parent)
        : ChildCommandOf<CommandAmaParticipantGroup>("auto-assign", "Update groups of non-test participants", parent) {
      }

    protected:
      pep::commandline::Parameters getSupportedParameters() const override {
        return ChildCommandOf<CommandAmaParticipantGroup>::getSupportedParameters()
          + pep::commandline::Parameter("mapname", "Use a different group name for a context name. Specify as \"contextName=groupName\"").value(pep::commandline::Value<std::string>().multiple())
          + pep::commandline::Parameter("wet", "Not a dry run: apply required changes");
      }

      int execute() override {
        const auto& vm = this->getParameterValues();
        return executeEventLoopFor([&vm](std::shared_ptr<pep::CoreClient> client) {
          auto context = ParticipantGroup::AutoAssignContext::Create(client, vm.has("wet"), vm.getOptionalMultiple<std::string>("mapname"));
          if (context->applyUpdates()) {
            std::cerr << "Performing a configuration run: updates will be applied." << std::endl;
          }
          else {
            std::cerr << "Performing a dry run: updates will be reported but not applied." << std::endl;
          }
          return ParticipantGroup::AutoAssign(context);
          });
      }
    };

  protected:
    inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#ama-group"; }

    std::vector<std::shared_ptr<Command>> createChildCommands() override {
      return {std::make_shared<AmaParticipantGroupCreateCommand>(*this),
              std::make_shared<AmaParticipantGroupRemoveSubCommand>(*this),
              std::make_shared<AmaParticipantGroupClearSubCommand>(*this),
              std::make_shared<AmaParticipantGroupingSubCommand>("addTo",
                                                                 "Add participant to group",
                                                                 &pep::AccessManagerProxy::amaAddParticipantToGroup,
                                                                 *this),
              std::make_shared<AmaParticipantGroupingSubCommand>("removeFrom",
                                                                 "Remove participant from group",
                                                                 &pep::AccessManagerProxy::amaRemoveParticipantFromGroup,
                                                                 *this),
              std::make_shared<AmaParticipantGroupAutoAssignCommand>(*this)};
    }
  };

protected:
  inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#ama"; }

  std::vector<std::shared_ptr<pep::commandline::Command>> createChildCommands() override {
    return {std::make_shared<CommandAmaQuery>(*this),
            std::make_shared<CommandAmaColumn>(*this),
            std::make_shared<CommandAmaColumnGroup>(*this),
            std::make_shared<CommandAmaParticipantGroup>(*this),
            std::make_shared<CommandAmaCgar>(*this),
            std::make_shared<CommandAmaPgar>(*this)};
  }
};

} // namespace

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandAma(CliApplication& parent) {
  return std::make_shared<CommandAma>(parent);
}
