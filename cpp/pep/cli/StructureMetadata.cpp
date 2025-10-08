#include <map>
#include <ranges>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <pep/application/Application.hpp>
#include <pep/async/RxUtils.hpp>
#include <pep/cli/Command.hpp>
#include <pep/cli/Commands.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/utils/Stream.hpp>

using namespace pep;
using namespace pep::cli;
using namespace std::string_literals;

namespace {

StructureMetadataKey ParseMetadataKey(std::string_view key, bool allowWildcard) {
  auto error = std::invalid_argument(
      allowWildcard ? "Metadata key should be of the form 'metadata_group:subkey' or 'metadata_group:*'"
                    : "Metadata key should be of the form 'metadata_group:subkey'");

  const auto separatorPos = key.find(':');
  if (separatorPos == std::string_view::npos) { throw error; }

  auto group = key.substr(0, separatorPos);
  auto subkey = key.substr(separatorPos + 1);

  if (group.empty() || subkey.empty() || group == "*") { throw error; }

  if (subkey == "*") {
    if (!allowWildcard) { throw error; }
    subkey = "";
  }

  return {
      .metadataGroup = std::string(group),
      .subkey = std::string(subkey),
  };
}

const std::map<std::string, StructureMetadataType> MetadataTypeMapping{
  {"column", StructureMetadataType::Column},
  {"column-group", StructureMetadataType::ColumnGroup},
  {"participant-group", StructureMetadataType::ParticipantGroup},
};

StructureMetadataType ParseMetadataType(const std::string& type) {
  if (auto it = MetadataTypeMapping.find(type);
    it != MetadataTypeMapping.end()) {
    return it->second;
  }
  throw std::invalid_argument("Unknown metadata type: "s += type);
}


class CommandStructureMetadata final : public ChildCommandOf<CliApplication> {
public:
  explicit CommandStructureMetadata(CliApplication& parent) : ChildCommandOf("structure-metadata",
    "Alters metadata for non-cell structures of the system", parent) {}

  StructureMetadataType getMetadataType() const {
    return ParseMetadataType(getParameterValues().get<std::string>("type"));
  }

protected:
  commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf::getSupportedParameters()
        + commandline::Parameter("type", "The structure type to alter metadata for")
        .value(commandline::Value<std::string>().positional().required()
          .allow(std::views::keys(MetadataTypeMapping)));
  }

  std::vector<std::shared_ptr<Command>> createChildCommands() override;
};

class CommandMetadataGet final : public ChildCommandOf<CommandStructureMetadata> {
public:
  explicit CommandMetadataGet(CommandStructureMetadata& parent)
    : ChildCommandOf("get", "Retrieves the content of a single metadata entry", parent) {}

protected:
  commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf::getSupportedParameters()
        + commandline::Parameter("subject", "Name of the structure we should retrieve metadata for")
              .value(commandline::Value<std::string>().positional().required())
        + commandline::Parameter("key", "Metadata key we should retrieve (metadata_group:subkey)")
              .value(commandline::Value<std::string>().required());
  }

  int execute() override {
    return this->executeEventLoopFor([subjectType = getParent().getMetadataType(),
                                      values = this->getParameterValues()](const std::shared_ptr<CoreClient>& client) {
      StructureMetadataSubjectKey requestedSubjectKey{
        .subject = values.get<std::string>("subject"),
        .key = ParseMetadataKey(values.get<std::string>("key"), false),
      };

      return client->getAccessManagerProxy()->getStructureMetadata(subjectType, {requestedSubjectKey.subject}, {requestedSubjectKey.key})
          .op(RxToVector())
          .map([requestedSubjectKey](const std::shared_ptr<std::vector<StructureMetadataEntry>>& entries) -> FakeVoid {
            if (entries->empty()) {
              throw std::runtime_error("Metadata entry does not exist");
            }
            if (entries->size() > 1) {
              throw std::runtime_error("Expected single metadata entry but server sent multiple");
            }
            auto& entry = entries->front();
            if (entry.subjectKey != requestedSubjectKey) {
              throw std::runtime_error(
                  "Expected single metadata entry "
                  + requestedSubjectKey.key.toString() + " for " + requestedSubjectKey.subject
                  + " but got " + entry.subjectKey.key.toString() + " for " + entry.subjectKey.subject);
            }
            auto setStdoutBinary = SetBinaryFileMode::ForStdout();
            std::cout << entry.value;
            return {};
          });
    });
  }
};

class CommandMetadataList final : public ChildCommandOf<CommandStructureMetadata> {
public:
  explicit CommandMetadataList(CommandStructureMetadata& parent)
    : ChildCommandOf("list", "Lists multiple metadata entries", parent) {}

protected:
  commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf::getSupportedParameters()
        + commandline::Parameter("subject", "Names of the structures we should retrieve metadata for")
              .value(commandline::Value<std::string>().positional().multiple())
        + commandline::Parameter("key", "Metadata keys we should retrieve (metadata_group:subkey or metadata_group:*)")
              .value(commandline::Value<std::string>().multiple())
        + commandline::Parameter("json", "Output as JSON instead of human-readable");
  }

  int execute() override {
    return this->executeEventLoopFor(
        [subjectType = getParent().getMetadataType(),
         values = this->getParameterValues()](const std::shared_ptr<CoreClient>& client) -> rxcpp::observable<FakeVoid> {
          auto subjects = values.getOptionalMultiple<std::string>("subject");
          auto keyStrs = values.getOptionalMultiple<std::string>("key");
          std::vector<StructureMetadataKey> keys;
          keys.reserve(keyStrs.size());
          std::ranges::transform(keyStrs, std::back_inserter(keys), [](std::string_view key) {
            return ParseMetadataKey(key, true);
          });

          bool json = values.has("json");

          auto getEntries = client->getAccessManagerProxy()->getStructureMetadata(subjectType, std::move(subjects), std::move(keys));

          if (json) {
            using namespace boost::property_tree;
            auto root = std::make_shared<ptree>();
            return getEntries.map([root](const StructureMetadataEntry& entry) -> FakeVoid {
              root->add(RawPtreePath(entry.subjectKey.subject) / RawPtreePath(entry.subjectKey.key.toString()), entry.value);
              return {};
            }).op(RxBeforeCompletion([root] {
              write_json(std::cout, *root);
            }));
          }
          else {
            auto root = std::make_shared<std::map<std::string /*subject*/, std::map<StructureMetadataKey, std::string /*value*/>>>();
            return getEntries.map([root](StructureMetadataEntry entry) -> FakeVoid {
              (*root)[std::move(entry.subjectKey.subject)][std::move(entry.subjectKey.key)] = std::move(entry.value);
              return {};
            }).op(RxBeforeCompletion([root] {
              for (const auto& [subject, meta] : *root) {
                std::cout << "==== " << subject << " ====\n";
                for (const auto& [key, value] : meta) {
                  auto valueTrunc = std::string_view(value).substr(0, 1'000);
                  std::cout << "- " << key.toString() << " = " << valueTrunc;
                  if (valueTrunc.size() < value.size()) {
                    std::cout << "[...truncated " << (value.size() - valueTrunc.size()) << " bytes]";
                  }
                  std::cout << '\n';
                }
                std::cout << '\n';
              }
            }));
          }
        });
  }
};

class CommandMetadataSet final : public ChildCommandOf<CommandStructureMetadata> {
public:
  explicit CommandMetadataSet(CommandStructureMetadata& parent) : ChildCommandOf("set", "Sets metadata", parent) {}

protected:
  commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf::getSupportedParameters()
        + commandline::Parameter("subject", "Name of the structure we should set metadata for")
              .value(commandline::Value<std::string>().positional().required())
        + commandline::Parameter("key", "Metadata key we should set (metadata_group:subkey)")
              .value(commandline::Value<std::string>().required())
        + commandline::Parameter("value", "Metadata value (read from stdin if omitted)").value(commandline::Value<std::string>());
  }

  int execute() override {
    return this->executeEventLoopFor([subjectType = getParent().getMetadataType(),
                                      values = this->getParameterValues()](const std::shared_ptr<CoreClient>& client) {
      auto subject = values.get<std::string>("subject");
      auto key = ParseMetadataKey(values.get<std::string>("key"), false);

      auto value = values.getOptional<std::string>("value");
      if (!value) {
        LOG(LOG_TAG, info) << "Reading value from stdin (use --value to specify in command instead)";

        auto setStdinBinary = SetBinaryFileMode::ForStdin();
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        value = std::move(ss).str();
      }

      return client->getAccessManagerProxy()->setStructureMetadata(subjectType, pep::messaging::MakeSingletonTail(
          StructureMetadataEntry{
            .subjectKey = {std::move(subject), std::move(key)},
            .value = std::move(*value),
          }));
    });
  }
};

class CommandMetadataRemove final : public ChildCommandOf<CommandStructureMetadata> {
public:
  explicit CommandMetadataRemove(CommandStructureMetadata& parent) : ChildCommandOf("remove", "Removes metadata", parent) {}

protected:
  commandline::Parameters getSupportedParameters() const override {
    return ChildCommandOf::getSupportedParameters()
        + commandline::Parameter("subject", "Name of the structure we should remove metadata for")
              .value(commandline::Value<std::string>().positional().required())
        + commandline::Parameter("key", "Metadata key we should remove (metadata_group:subkey)")
              .value(commandline::Value<std::string>().required());
  }

  int execute() override {
    return this->executeEventLoopFor([subjectType = getParent().getMetadataType(),
                                      values = this->getParameterValues()](const std::shared_ptr<CoreClient>& client) {
      auto subject = values.get<std::string>("subject");
      auto key = ParseMetadataKey(values.get<std::string>("key"), false);

      return client->getAccessManagerProxy()->removeStructureMetadata(subjectType, {
          {std::move(subject), std::move(key)},
      });
    });
  }
};

} // namespace

std::vector<std::shared_ptr<commandline::Command>> CommandStructureMetadata::createChildCommands() {
  return {
      std::make_shared<CommandMetadataList>(*this),
      std::make_shared<CommandMetadataGet>(*this),
      std::make_shared<CommandMetadataSet>(*this),
      std::make_shared<CommandMetadataRemove>(*this),
  };
}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandStructureMetadata(CliApplication& parent) {
  return std::make_shared<CommandStructureMetadata>(parent);
}
