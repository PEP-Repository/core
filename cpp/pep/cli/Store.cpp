#include <pep/cli/Commands.hpp>
#include <pep/cli/SingleCellCommand.hpp>
#include <pep/utils/File.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/archiving/Pseudonymiser.hpp>
#include <pep/archiving/Tar.hpp>
#include <pep/async/RxBeforeCompletion.hpp>
#include <pep/async/RxGetOne.hpp>
#include <pep/utils/Stream.hpp>
#include <pep/morphing/MorphingSerializers.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <iostream>
#include <filesystem>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include <boost/algorithm/string/join.hpp>
#include <boost/property_tree/ptree.hpp>
#include <pep/messaging/MessageSequence.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-switch_if_empty.hpp>
#include <rxcpp/operators/rx-take.hpp>

using namespace pep::cli;
namespace pt = boost::property_tree;

using pep::cli::LOG_TAG;

namespace {

struct StoreContext {
  std::shared_ptr<pep::PolymorphicPseudonym> pp;
  std::string column;
  std::filesystem::path inputPath{};
  bool requiresDirectory{false};
  bool shouldResolveSymlinks{false};
  std::optional<std::string> data{};
  std::optional<std::string> pseudonym{};

  std::map<std::string, pep::MetadataXEntry> meta{};
};

struct PathStreamPair {
  std::filesystem::path path;
  std::shared_ptr<std::ifstream> stream;
};

void AddSpecifiedMetadata(std::map<std::string, pep::MetadataXEntry>& metadata, const pep::commandline::NamedValues& parameterValues) {
  std::string extension{};
  if (parameterValues.has("file-extension")) {
    extension = parameterValues.get<std::string>("file-extension");
    if (!extension.empty() && !pep::IsValidFileExtension(extension)) {
      throw std::runtime_error("Please specify either an empty string, or a valid file extension including the leading period/dot character");
    }
  }
  else if (parameterValues.has("input-path")) {
    auto path = std::filesystem::path(parameterValues.get<std::string>("input-path"));
    extension = path.extension().string();
  }

  if (!extension.empty()) {
    metadata.emplace(pep::MetadataXEntry::MakeFileExtension(extension));
  }

  // parse and add extra metadata entries
  for (auto&& json
    : parameterValues.getOptionalMultiple<std::string>("metadataxentry")) {

    pep::proto::NamedMetadataXEntry e;
    auto status = google::protobuf::util::JsonStringToMessage(json, &e);
    if (!status.ok()) {
      std::stringstream ss;
      ss << "Parsing metadata entry " << std::quoted(json) << " failed";
      ss << ": " << status;
      throw std::runtime_error(std::move(ss).str());
    }

    auto entry = pep::Serialization::FromProtocolBuffer(std::move(*e.mutable_value()));
    if (entry.storeEncrypted() || entry.bound()) {
      throw std::runtime_error("Encrypted or bound metadata are currently not supported.");
    }

    auto [it, inserted] = metadata.insert({
      e.name(),
      entry
      });
    if (!inserted) {
      std::stringstream ss;
      ss << "metadata entry " << std::quoted(e.name())
        << " specified twice.";
      throw std::runtime_error(std::move(ss).str());
    }
  }
}

/* \brief Takes the parameters provided by the user, global configuration, pp, and column and puts them in a single struct. */
rxcpp::observable < std::shared_ptr<StoreContext>> CreateContext(std::shared_ptr<pep::CoreClient> client, const pep::commandline::NamedValues& parameterValues, std::shared_ptr<pep::PolymorphicPseudonym> pp, const std::string& column) {
  auto context = std::make_shared<StoreContext>();
  // Get local context parameters
  context->inputPath = std::filesystem::path{parameterValues.get<std::string>("input-path")};
  context->pp = pp;
  context->column = column;
  if (parameterValues.has("data")) {
    context->data = parameterValues.get<std::string>("data");
  }
  context->shouldResolveSymlinks = parameterValues.has("resolve-symlinks");

  AddSpecifiedMetadata(context->meta, parameterValues);

  // Get optional serverside parameters
  return client->getGlobalConfiguration()
    .flat_map([client, context](std::shared_ptr<pep::GlobalConfiguration> globalConfig) {

    if (auto columnSpec = globalConfig->getColumnSpecification(context->column)) {
      if (columnSpec->getRequiresDirectory()) {
        context->requiresDirectory = true;

        auto emplaced = context->meta.emplace(pep::MetadataXEntry::MakeFileExtension(".tar")).second; // Adding a extension so unpacking the archive file can go directly to the destination without extension.
          if (!emplaced) {
            throw std::runtime_error("Please do not add the metadata key: 'fileExtension' when uploading directories.");
          }
        if (!std::filesystem::is_directory(context->inputPath)) {
          throw std::runtime_error("The given input path '" +context->inputPath.string() + "' should be a directory.");
        }
      }
      else {
        if (!std::filesystem::is_regular_file(std::filesystem::canonical(context->inputPath))) {
          throw std::runtime_error("The given input path '" + context->inputPath.string() + "' should  be a single file.");
        }
      }
      if (auto spColumn = columnSpec->getAssociatedShortPseudonymColumn()) {
        pep::enumerateAndRetrieveData2Opts opts;
        opts.pps = {*context->pp};
        opts.columns = {*spColumn};
        return client->enumerateAndRetrieveData2(opts)
          .op(pep::RxGetOne("short pseudonym result"))
          .map([context](pep::EnumerateAndRetrieveResult result) {
          assert(!context->pseudonym.has_value());
          context->pseudonym = result.mData;
          auto placeholder = pep::Pseudonymiser::GetDefaultPlaceholder().substr(0, context->pseudonym->length());
          std::string placeholderKey{"pseudonymPlaceholder"};

          auto emplaced = context->meta.emplace(pep::NamedMetadataXEntry{placeholderKey, pep::MetadataXEntry::FromPlaintext(std::move(placeholder), false, false)}).second;
          if (!emplaced) {
            throw std::runtime_error("Please do not add the metadata key: '" + placeholderKey + "' as it is a reserved keyword.");
          }
          return context;
        })
        .as_dynamic();
      }
    }
  return rxcpp::observable<>::just(context).as_dynamic();
  });
}

void CheckSymlinkAllowed(const std::filesystem::path& inpath, bool shouldResolveSymlinks) {
  if (shouldResolveSymlinks) {
    // Symlinks allowed so Tthere is nothing to check.
    return;
  }
  std::vector<std::filesystem::path> foundSymlinks{};
  if (std::filesystem::is_symlink(inpath))
  {
    foundSymlinks.push_back(inpath);
  }
  if (std::filesystem::is_directory(inpath)) {
    for (auto iterator = std::filesystem::recursive_directory_iterator(inpath, std::filesystem::directory_options::follow_directory_symlink); iterator != std::filesystem::recursive_directory_iterator{}; iterator++) {
      if (std::filesystem::is_symlink(iterator->path())) {
        foundSymlinks.push_back(iterator->path());
      }
    }
  }

  if (foundSymlinks.size() > 0) {
    std::ostringstream message;
    message << "Symbolic link(s) found. By default this is not supported for pseudonymization.\n If symlinks should be resolved, please add the resolve-symlinks flag to the store command.\n Symlinks found at:\n";
    for (auto &path : foundSymlinks){
      message << path.string() << "\n";
    }
    throw std::runtime_error(message.str());
  }
}

std::filesystem::path FindUnusedPath(const std::filesystem::path& path) {
  std::string extension = ".tmp";
  auto basepath = path.string();
  if (!basepath.empty()) {
    if (basepath.back() == '/') {
      basepath.pop_back();
    }
  }
  auto outpath = basepath + extension;
  for (size_t i = 1; std::filesystem::exists(outpath); ++i) {
    outpath = basepath + " " + std::to_string(i) + extension;
  }

  return std::filesystem::path(outpath);
}

std::filesystem::path CreatePseudonymizedFileToUpload(std::shared_ptr<StoreContext> context) {
  CheckSymlinkAllowed(context->inputPath, context->shouldResolveSymlinks);

  const auto& inputPath = context->inputPath;
  auto outputPath = FindUnusedPath(inputPath);
  auto out = std::make_shared<std::ofstream>(outputPath.string(), std::ios::binary);

  std::optional<pep::Pseudonymiser> pseudonymiser{std::nullopt};
  if (context->requiresDirectory) {
    auto tar = pep::Tar::Create(out);
    if (context->pseudonym.has_value()) {
      pseudonymiser = pep::Pseudonymiser(*context->pseudonym);
    }
    WriteToArchive(inputPath, tar, pseudonymiser);
  }
  else {
    // Single File that needs pseudonymisation
    assert(context->pseudonym.has_value());
    std::ifstream in{inputPath.string(), std::ios::binary};
    auto writeToStream = [&out](const char* c, const std::streamsize l) {out->write(c, l); out->flush(); };
    pseudonymiser = pep::Pseudonymiser(*context->pseudonym);
    pseudonymiser->pseudonymise(in, writeToStream);

  }
  return outputPath;
}

class CommandStore : public SingleCellModificationCommand {
public:
  explicit CommandStore(CliApplication& parent)
    : SingleCellModificationCommand("store", "Store a file", parent) {
  }

private:
  static std::string GetRequiredDataSourceMessage() {
    return "Please specify exactly one of --input-path, or --data, or --metadata-only";
  }

  rxcpp::observable<pep::DataStorageResult2> storeNewCellData(std::shared_ptr<pep::CoreClient> client, const pep::storeData2Opts& opts, std::shared_ptr<pep::PolymorphicPseudonym> pp, const std::string& column) {
    auto cleanupFiles = std::make_shared<std::vector<PathStreamPair>>();
    return CreateContext(client, this->getParameterValues(), pp, column)
      .flat_map([client, opts, cleanupFiles](std::shared_ptr<StoreContext> context) {
        pep::messaging::MessageBatches batches;

        if (context->data.has_value()) {
          batches = rxcpp::observable<>::just(rxcpp::observable<>::just(std::make_shared<std::string>(*context->data)).as_dynamic());
        }
        else {
          std::shared_ptr<std::istream> stream;

          std::shared_ptr<std::optional<pep::SetBinaryFileMode>> setStdinBinary;
          if (context->pseudonym.has_value() || context->requiresDirectory) {
            auto path = CreatePseudonymizedFileToUpload(context);
            // The stream object will be held alive and therefore open by rxcpp for too long, hindering deleting the file.
            // This extra pointer fileStream (pointer to iFstream, not istream) is given to the cleanupFiles to manually close the stream if necessary.
            auto fileStream = std::make_shared<std::ifstream>(path.string(), std::ios_base::in | std::ios_base::binary);
            cleanupFiles->push_back(PathStreamPair{path, fileStream});
            stream = fileStream;
          }
          else {
            auto path = context->inputPath;
            if (path.string() == "-") {
              setStdinBinary = std::make_shared<std::optional<pep::SetBinaryFileMode>>(pep::SetBinaryFileMode::ForStdin());
              stream = std::shared_ptr<std::istream>(&std::cin, [](void*) {});
            }
            else {
              stream = std::make_shared<std::ifstream>(path.string(), std::ios_base::in | std::ios_base::binary);
            }
          }
          batches = pep::messaging::IStreamToMessageBatches(stream);
          if (setStdinBinary) {
            batches = batches.op(pep::RxBeforeCompletion([setStdinBinary] {
              setStdinBinary->reset();
            }));
          }
        }

        pep::StoreData2Entry entry(context->pp, context->column, batches);
        entry.mXMetadata = context->meta;
        return client->storeData2({ entry }, opts);
        })
        .op(pep::RxBeforeTermination([cleanupFiles](std::optional<std::exception_ptr>) {
          for (auto& entry : *cleanupFiles) {
            try {
              if (entry.stream->is_open()) {
                entry.stream->close();
              }
              std::filesystem::remove(entry.path);
            }
            catch (std::exception& e) {
              LOG(LOG_TAG, pep::warning) << "Could not remove temporary file " << entry.path << ": " << e.what();
            }
          }
        }));
  }

  rxcpp::observable<pep::DataStorageResult2> updateCellMetadata(std::shared_ptr<pep::CoreClient> client, const pep::storeData2Opts& opts, std::shared_ptr<pep::PolymorphicPseudonym> pp, const std::string& column) {
    pep::StoreMetadata2Entry entry(pp, column);
    AddSpecifiedMetadata(entry.mXMetadata, this->getParameterValues());
    return client->updateMetadata2({ entry }, opts);
  }

protected:
  inline std::optional<std::string> getAdditionalDescription() const override {
    std::vector<std::string> lines;
    auto parent = SingleCellModificationCommand::getAdditionalDescription();
    if (parent.has_value()) {
      lines.emplace_back(std::move(*parent));
    }
    lines.emplace_back(GetRequiredDataSourceMessage());
    return boost::algorithm::join(lines, "\n");
  }

  inline std::optional<std::string> getRelativeDocumentationUrl() const override { return "using-pepcli#store"; }

  pep::commandline::Parameters getSupportedParameters() const override {
    return SingleCellModificationCommand::getSupportedParameters()
      + pep::commandline::Parameter("input-path", "Path to read data from").alias("input-file").shorthand('i').value(pep::commandline::Value<std::string>().defaultsTo("-", "stdin"))
      + pep::commandline::Parameter("data", "Data to store").shorthand('d').value(pep::commandline::Value<std::string>())
      + pep::commandline::Parameter("metadata-only", "Store metadata only")
      + pep::commandline::Parameter("metadataxentry", "Specify extra metadata entries: --metadataxentry \"$(./pepcli xentry ...  )\"").shorthand('x').value(pep::commandline::Value<std::string>().multiple())
      + pep::commandline::Parameter("file-extension", "File extension that is appended when this data is pulled").value(pep::commandline::Value<std::string>())
      + pep::commandline::Parameter("resolve-symlinks", "Symlinks in the data should be resolved and followed. If this flag is not set and symlinks are found, execution is halted.");
  }

  void finalizeParameters() override {
    std::optional<std::filesystem::path> path;

    // Store explicitly specified input path before default is applied
    const auto& parameterValues = this->getParameterValues();
    if (parameterValues.has("input-path")) {
      path = parameterValues.get<std::string>("input-path");
    }

    // Apply defaults
    SingleCellModificationCommand::finalizeParameters();

    // Check parameter sanity
    unsigned sources = 0U;
    if (path.has_value()) {
      ++sources;
    }
    if (parameterValues.has("data")) {
      ++sources;
    }
    if (parameterValues.has("metadata-only")) {
      ++sources;
    }
    if (sources != 1U) {
      throw std::runtime_error(GetRequiredDataSourceMessage());
    }

    path = parameterValues.get<std::string>("input-path");
    if (path != "-" && !std::filesystem::exists(*path)) {
      throw std::runtime_error("Switch --input-path: '" + path->string() + "' does not exist");
    }
  }

  std::vector<std::string> ticketAccessModes() const override {
    if (this->getParameterValues().has("metadata-only")) {
      return { "read", "write-meta" };
    }
    return SingleCellModificationCommand::ticketAccessModes();
  }

  rxcpp::observable<pep::FakeVoid> performModification(std::shared_ptr<pep::CoreClient> client, const pep::storeData2Opts& opts, std::shared_ptr<pep::PolymorphicPseudonym> pp, const std::string& column) override {
    rxcpp::observable<pep::DataStorageResult2> store;
    if (this->getParameterValues().has("metadata-only")) {
      store = this->updateCellMetadata(client, opts, pp, column);
    }
    else {
      store = this->storeNewCellData(client, opts, pp, column);
    }

    return store
      .op(pep::RxGetOne("storage result"))
      .map([](pep::DataStorageResult2 res) {
        pt::ptree out;
        out.put("id", boost::algorithm::hex(res.mIds[0]));
        pt::write_json(std::cout, out);
        return pep::FakeVoid();
      });
  }
};

}

std::shared_ptr<ChildCommandOf<CliApplication>> pep::cli::CreateCommandStore(CliApplication& parent) {
  return std::make_shared<CommandStore>(parent);
}
