#include <pep/utils/Configuration.hpp>
#include <pep/versioning/Version.hpp>
#include <Messages.pb.checksum.h>

#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/facilities/is_empty.hpp>

#include <boost/algorithm/hex.hpp>

using namespace pep;

#ifndef PEP_VERSION_MAJOR
#  error Define PEP_VERSION_MAJOR before including this file.
#endif
#ifndef PEP_VERSION_MINOR
#  error Define PEP_VERSION_MINOR before including this file.
#endif
#ifndef PEP_VERSION_BUILD
#  error Define PEP_VERSION_BUILD before including this file.
#endif
#ifndef PEP_VERSION_REVISION
#  error Define PEP_VERSION_REVISION before including this file.
#endif

#pragma message("BinaryVersion::ProjectPath() = " BUILD_PROJECT_PATH)
#pragma message("BinaryVersion::Reference() = " BUILD_REF)
#pragma message("BinaryVersion::Major() = " BOOST_PP_STRINGIZE(PEP_VERSION_MAJOR))
#pragma message("BinaryVersion::Minor() = " BOOST_PP_STRINGIZE(PEP_VERSION_MINOR))
#pragma message("BinaryVersion::Build() = " BOOST_PP_STRINGIZE(PEP_VERSION_BUILD))
#pragma message("BinaryVersion::Revision() = " BOOST_PP_STRINGIZE(PEP_VERSION_REVISION))
#pragma message("BinaryVersion::Commit() = " BUILD_COMMIT)
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

const BinaryVersion BinaryVersion::current(BUILD_PROJECT_PATH, BUILD_REF, BUILD_COMMIT, PEP_VERSION_MAJOR, PEP_VERSION_MINOR, PEP_VERSION_BUILD, PEP_VERSION_REVISION, BUILD_TARGET, GetCurrentProtocolChecksum());
std::optional<std::filesystem::path> ConfigVersion::loadDir_;
std::optional<ConfigVersion> ConfigVersion::loaded_;


BinaryVersion::BinaryVersion(
    std::string projectPath,
    std::string reference,
    std::string commit,
    unsigned int majorVersion, 
    unsigned int minorVersion,
    unsigned int build,
    unsigned int revision,
    std::string target,
    std::string protocolChecksum)
  : GitlabVersion(
      std::move(projectPath),
      std::move(reference),
      std::move(commit),
      majorVersion,
      minorVersion,
      build,
      revision
      ),
    mTarget(std::move(target)),
    mProtocolChecksum(std::move(protocolChecksum)) {
}

std::string BinaryVersion::getSummary() const {
  auto spec = ConcatSummaryParts(this->constructSummary(std::nullopt, false), " - ", mProtocolChecksum);
  return ConcatSummaryParts(spec, " ", "(" + mTarget + ")");
}

std::string BinaryVersion::prettyPrint() const {
  std::stringstream result;
  result 
    << "Binary version for " << getTarget() << '\n'
    << GitlabVersion::prettyPrint()
    << "ProtocolChecksum: " << getProtocolChecksum() << '\n';
  return result.str();
}

ConfigVersion::ConfigVersion(
    std::string projectPath,
    std::string reference,
    std::string commit,
    unsigned int majorVersion,
    unsigned int minorVersion,
    unsigned int build,
    unsigned int revision,
    std::string projectCaption)
  : GitlabVersion(
      std::move(projectPath),
      std::move(reference),
      std::move(commit),
      majorVersion,
      minorVersion,
      build,
      revision
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
        config.get<std::string>("commit"),
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
  std::stringstream result;
  result 
    << "Project version for " << getProjectCaption() <<" (" << getReference() << ")"<< '\n'
    << GitlabVersion::prettyPrint();
  return result.str();
}
