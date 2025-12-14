#include <pep/utils/Configuration.hpp>
#include <pep/versioning/Version.hpp>
#include <Messages.pb.checksum.h>

#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>

#include <boost/algorithm/hex.hpp>

using namespace pep;

#ifndef BUILD_PIPELINE_ID
#  error Define BUILD_PIPELINE_ID before including this file.
#endif
#ifndef BUILD_JOB_ID
#  error Define BUILD_JOB_ID before including this file.
#endif

#ifndef BUILD_MAJOR_VERSION
#  error Define BUILD_MAJOR_VERSION before including this file.
#endif
#ifndef BUILD_MINOR_VERSION
#  error Define BUILD_MINOR_VERSION before including this file.
#endif

#pragma message("BinaryVersion::ProjectPath() = " BUILD_PROJECT_PATH)
#pragma message("BinaryVersion::Reference() = " BUILD_REF)
#pragma message("BinaryVersion::Major() = " BOOST_PP_STRINGIZE(BUILD_MAJOR_VERSION))
#pragma message("BinaryVersion::Minor() = " BOOST_PP_STRINGIZE(BUILD_MINOR_VERSION))
#pragma message("BinaryVersion::PipelineId() = " BOOST_PP_STRINGIZE(BUILD_PIPELINE_ID))
#pragma message("BinaryVersion::JobId() = " BOOST_PP_STRINGIZE(BUILD_JOB_ID))
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

const BinaryVersion BinaryVersion::current(BUILD_PROJECT_PATH, BUILD_REF, BUILD_REV, BUILD_MAJOR_VERSION, BUILD_MINOR_VERSION, BUILD_PIPELINE_ID, BUILD_JOB_ID,  BUILD_TARGET, GetCurrentProtocolChecksum());
std::optional<std::filesystem::path> ConfigVersion::loadDir_;
std::optional<ConfigVersion> ConfigVersion::loaded_;


BinaryVersion::BinaryVersion(
    std::string projectPath,
    std::string reference,
    std::string revision,
    unsigned int majorVersion,
    unsigned int minorVersion,
    unsigned int pipelineId,
    unsigned int jobId,
    std::string target,
    std::string protocolChecksum)
  : GitlabVersion(
      std::move(projectPath),
      std::move(reference),
      std::move(revision),
      majorVersion,
      minorVersion,
      pipelineId,
      jobId
      ),
    mTarget(std::move(target)),
    mProtocolChecksum(std::move(protocolChecksum)) {
}

std::string BinaryVersion::getSummary() const {
  auto spec = ConcatSummaryParts(this->constructSummary(std::nullopt, false), " - ", mProtocolChecksum);
  return ConcatSummaryParts(spec, " ", "(" + mTarget + ")");
}

std::string BinaryVersion::prettyPrint() const {
  std::ostringstream result;
  result
    << "Binary version for " << getTarget() << '\n'
    << GitlabVersion::prettyPrint()
    << "ProtocolChecksum: " << getProtocolChecksum() << '\n';
  return std::move(result).str();
}

ConfigVersion::ConfigVersion(
    std::string projectPath,
    std::string reference,
    std::string revision,
    unsigned int majorVersion,
    unsigned int minorVersion,
    unsigned int pipelineId,
    unsigned int jobId,
    std::string projectCaption)
  : GitlabVersion(
      std::move(projectPath),
      std::move(reference),
      std::move(revision),
      majorVersion,
      minorVersion,
      pipelineId,
      jobId
      ),
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
        config.get<std::string>("revision"),
        config.get<unsigned int>("majorVersion"),
        config.get<unsigned int>("minorVersion"),
        config.get<unsigned int>("pipelineId"),
        config.get<unsigned int>("jobId"),
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

std::string ConfigVersion::prettyPrint() const {
  std::ostringstream result;
  result
    << "Project version for " << getProjectCaption() <<" (" << getReference() << ")"<< '\n'
    << GitlabVersion::prettyPrint();
  return std::move(result).str();
}
