#include <pep/utils/Configuration.hpp>
#include <pep/versioning/Version.hpp>
#include <Messages.pb.checksum.h>

#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>

#include <boost/algorithm/hex.hpp>

using namespace pep;

// Get string versions of externally-provided unquoted (integer) macros.
// Checking for empty preprocessor symbol: https://stackoverflow.com/a/15380877
#if BOOST_PP_IS_EMPTY(BUILD_PIPELINE_ID)
# define BUILD_PIPELINE ""
#else
# define BUILD_PIPELINE BOOST_PP_STRINGIZE(BUILD_PIPELINE_ID)
#endif

#if BOOST_PP_IS_EMPTY(BUILD_JOB_ID)
# define BUILD_JOB  ""
#else
# define BUILD_JOB  BOOST_PP_STRINGIZE(BUILD_JOB_ID)
#endif

#pragma message("BinaryVersion::ProjectPath() = " BUILD_PROJECT_PATH)
#pragma message("BinaryVersion::Reference() = " BUILD_REF)
#pragma message("BinaryVersion::PipelineId() = " BUILD_PIPELINE)
#pragma message("BinaryVersion::JobId() = " BUILD_JOB)
#pragma message("BinaryVersion::Revision() = " BUILD_REV)
#pragma message("BinaryVersion::Target() = " BUILD_TARGET)
#pragma message("BinaryVersion::MessagesProtoFileChecksum() = " MESSAGES_PROTO_CHECKSUM)

namespace {
  const uint8_t MANUAL_PROTOCOL_CHECKSUM_COMPONENT = 2U; // Increase when client+server software need to be upgraded in a big bang. Cycle back to 1 (or 0) if necessary

  std::string GetCurrentProtocolChecksum() {
    // Convert manual checksum component to hex string
    auto manual = boost::algorithm::hex_lower(std::string(reinterpret_cast<const char*>(&MANUAL_PROTOCOL_CHECKSUM_COMPONENT), 1));
    // Append calculated checksum for Messages.proto
    auto complete = manual + MESSAGES_PROTO_CHECKSUM;
    // Crop result for readability
    return complete.substr(0, 20);
  }
}

const BinaryVersion BinaryVersion::current(BUILD_PROJECT_PATH, BUILD_REF, BUILD_PIPELINE, BUILD_JOB, BUILD_REV, BUILD_TARGET, GetCurrentProtocolChecksum());
std::optional<std::filesystem::path> ConfigVersion::loadDir_;
std::optional<ConfigVersion> ConfigVersion::loaded_;


BinaryVersion::BinaryVersion(
    std::string projectPath,
    std::string reference,
    std::string pipelineId,
    std::string jobId,
    std::string revision,
    std::string target,
    std::string protocolChecksum)
  : GitlabVersion(
      std::move(projectPath),
      std::move(reference),
      std::move(pipelineId),
      std::move(jobId),
      std::move(revision)),
    mTarget(std::move(target)),
    mProtocolChecksum(std::move(protocolChecksum)) {
}

std::string BinaryVersion::getSummary() const {
  auto spec = ConcatSummaryParts(this->constructSummary(std::nullopt, false), " - ", mProtocolChecksum);
  return ConcatSummaryParts(spec, " ", "(" + mTarget + ")");
}

ConfigVersion::ConfigVersion(
    std::string projectPath,
    std::string reference,
    std::string pipelineId,
    std::string jobId,
    std::string revision,
    std::string projectCaption)
  : GitlabVersion(
      std::move(projectPath),
      std::move(reference),
      std::move(pipelineId),
      std::move(jobId),
      std::move(revision)),
    mProjectCaption(std::move(projectCaption)) {
}

std::optional<ConfigVersion> ConfigVersion::Current() {
  return loaded_;
}

std::optional<ConfigVersion> ConfigVersion::TryLoad(const std::filesystem::path& directory) {
  // std::filesystem::absolute throws when called with an empty string argument
  auto abs = directory != "" ? std::filesystem::absolute(directory) : "";

  if (loadDir_.has_value()) { // This function has been called before
    if (abs != *loadDir_) {
      throw std::runtime_error("Version file has already been loaded from directory " + loadDir_->string());
    }
  }
  else {
    loadDir_ = abs;
    auto file = directory / "configVersion.json";
    if (std::filesystem::exists(file)) {
      auto config = Configuration::FromFile(file);
      loaded_ = ConfigVersion(
        config.get<std::string>("projectPath"),
        config.get<std::string>("reference"),
        config.get<std::string>("pipelineId"),
        config.get<std::string>("jobId"),
        config.get<std::string>("revision"),
        config.get<std::string>("projectCaption")
      );
    }
  }

  return loaded_;
}

bool ConfigVersion::exposesProductionData() const noexcept {
  return this->getReference() == "prod";
}

std::string ConfigVersion::getSummary() const {
  return this->constructSummary(this->getProjectCaption(), true);
}
