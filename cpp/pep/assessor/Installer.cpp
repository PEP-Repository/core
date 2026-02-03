#include <pep/utils/Paths.hpp>
#include <pep/versioning/Version.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/utils/Win32Api.hpp>
#include <pep/assessor/Installer.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QFileDialog>

#ifdef _WIN32

/* Macro PEP_INSTALLER_BROWSE_LOCALLY is a development and debug helper.
 * If defined, the installer (and its metadata file) aren't downloaded from the PEP server.
 * Instead the user can browse for a (pep).msi file on the local computer, which is then
 * considered to supersede the currently running software version.
 *
 * DO NOT PUSH macro definitions to the central git(lab) environment, since that would
 * break the intallation and update mechanism of our releases.
 */
// #define PEP_INSTALLER_BROWSE_LOCALLY

/* Macro PEP_INSTALLER_ALWAYS_PROMPT_FOR_CREDENTIALS is a development and debug helper.
 * If defined, all users are prompted for (administrative) credentials, even if they
 * can elevate or are already running under administrative privileges.
 *
 * DO NOT PUSH macro definitions to the central git(lab) environment, since that would
 * break the intallation and update mechanism of our releases.
 */
// #define PEP_INSTALLER_ALWAYS_PROMPT_FOR_CREDENTIALS

namespace {
  const std::string LOG_TAG = "Installer";

  class PublishedInstaller : public Installer {
  public:
    struct Context {
      std::filesystem::path logDirectory;
      std::filesystem::path elevateExe;
      std::function<pep::win32api::PlaintextCredentials()> getAdministrativeCredentials;
    };

  private:
    std::shared_ptr<boost::property_tree::ptree> mProperties;

  private:
    PublishedInstaller(std::shared_ptr<boost::property_tree::ptree> properties)
      : mProperties(properties) {
    }

    static std::string GetDownloadUrl();

  protected:
    std::filesystem::path getLocalMsiPath() const override;

  public:
    unsigned getMajorVersion() const override { return mProperties->get<unsigned>("installer.major"); }
    unsigned getMinorVersion() const override { return mProperties->get<unsigned>("installer.minor"); }
    unsigned getBuild() const override { return mProperties->get<unsigned>("installer.pipeline"); }
    unsigned getRevision() const override { return mProperties->get<unsigned>("installer.job"); }

    bool supersedesRunningVersion() const override;
    static std::shared_ptr<PublishedInstaller> GetAvailable();
  };

  bool PublishedInstaller::supersedesRunningVersion() const {
    auto installerSemver = getSemver();
    auto current = pep::ConfigVersion::Current();
    assert(current != std::nullopt);
    return installerSemver > current->getSemver();
  }

#ifdef PEP_INSTALLER_BROWSE_LOCALLY

  class LocalInstaller : public Installer {
  private:
    std::filesystem::path mPath;

  private:
    explicit LocalInstaller(const std::string& path)
      : mPath(path) {
    }

  protected:
    std::filesystem::path getLocalMsiPath() const override { return mPath; }

  public:
    unsigned getMajorVersion() const override { return 0U; }
    unsigned getMinorVersion() const override { return 0U; }
    unsigned getBuild() const override { return 0U; }
    virtual unsigned getRevision() const { return 0U; }
    virtual bool supersedesRunningVersion() const { return true; }

    static std::shared_ptr<LocalInstaller> GetAvailable() {
      auto dir = pep::GetExecutablePath().parent_path().string();
      auto path = QFileDialog::getOpenFileName(nullptr, "Select available installer", QString::fromStdString(dir), "Windows Installer archives (*.msi);;All Files (*)");
      if (path.isEmpty()) {
        return nullptr;
      }
      return std::shared_ptr<LocalInstaller>(new LocalInstaller(path.toStdString()));
    }
  };

#endif

  std::string PublishedInstaller::GetDownloadUrl() {
    auto version = pep::ConfigVersion::Current();
    if (version == std::nullopt) {
      throw std::runtime_error("No configuration version available to determine download URL");
    }
    auto projectCaption = version->getProjectCaption();
    auto reference = version->getReference();
    if (projectCaption.empty() || reference.empty()) {
      throw std::runtime_error("Configuration version does not specify projectCaption and/or reference, needed to determine download URL");
    }
    return "https://pep.cs.ru.nl/" + projectCaption + "/" + reference;
  }

  std::shared_ptr<PublishedInstaller> PublishedInstaller::GetAvailable() {
    auto version = pep::ConfigVersion::Current();
    if (version == std::nullopt) {
      LOG(LOG_TAG, pep::debug) << "Cannot determine configuration version. Not retrieving installer properties.";
      return nullptr;
    }
    if (!version->isGitlabBuild()) {
      LOG(LOG_TAG, pep::debug) << "Manual build - running debug session? Not retrieving installer properties.";
      return nullptr;
    }

    try {
      //Read from updateURL
      auto metaUrl = GetDownloadUrl() + "/installer.xml";
      auto installerMetaXmlFile = pep::win32api::GetUniqueTemporaryFileName();
      auto installerMetaXmlFileLocation = installerMetaXmlFile.string();
      PEP_DEFER(std::remove(installerMetaXmlFileLocation.c_str()));

      pep::win32api::Download(metaUrl, installerMetaXmlFile);

      auto updateXMLTree = std::make_shared<boost::property_tree::ptree>();
      boost::property_tree::read_xml(installerMetaXmlFileLocation, *updateXMLTree);
      return std::shared_ptr<PublishedInstaller>(new PublishedInstaller(updateXMLTree));
    }
    catch (std::exception& e) {
      LOG(LOG_TAG, pep::error) << "Error retrieving installer properties: " << e.what();
    }
    return nullptr;
  }

  std::filesystem::path PublishedInstaller::getLocalMsiPath() const {
    auto directory = pep::win32api::CreateTemporaryDirectory(); // TODO: delete when done
    std::filesystem::path result;

    for (const auto& node : mProperties->get_child("installer.files")) {
      auto relative = node.second.get<std::string>("path");
      if (!boost::iends_with(relative, std::string(".msi"))) {
        throw std::runtime_error("Only .MSI update files are currently supported");
      }
      auto source = GetDownloadUrl() + "/" + relative;
      auto destination = directory / relative;
      pep::win32api::Download(source, destination);

      if (node.second.get<std::string>("hash.<xmlattr>.algorithm") != "sha512") {
        throw std::runtime_error("Updating currently only supports SHA-512 hashing");
      }
      auto hash = boost::algorithm::unhex(node.second.get<std::string>("hash"));

      std::ifstream file(destination.string(), std::ifstream::binary);
      if (!file) {
        throw std::runtime_error("Could not open downloaded file " + relative + " to check hash");
      }

      std::vector<char> buffer(1024);
      pep::Sha512 sha;
      while (file.read(buffer.data(), buffer.size())) {
        sha.update(reinterpret_cast<uint8_t *>(buffer.data()), file.gcount());
      }
      // Process last chunk if nonempty. See pep/core#2076
      auto bytes_remaining = file.gcount();
      if (bytes_remaining != 0U) {
        sha.update(reinterpret_cast<uint8_t*>(buffer.data()), file.gcount());
      }

      if (sha.digest() != hash) {
        throw std::runtime_error("File " + relative + " was not downloaded correctly");
      }

      if (!result.empty()) {
        throw std::runtime_error("Only single-file updates are supported at this time");
      }
      result = destination;
    }

    if (result.empty()) {
      throw std::runtime_error("Could not determine how to start update process");
    }

    return result;
  }

}

void Installer::start(const Context& context) const {
  auto path = this->getLocalMsiPath();
  assert(boost::iequals(path.extension().string(), ".msi"));

  auto msi = path.string();
  boost::replace_all(msi, "/", "\\"); // MSIexec fails if the / i parameter is passed with forward slashes as directory delimiters

  auto logfile = context.logDirectory / "install.log";
  std::string cmd = "msiexec.exe";
  auto parameters = "/i \"" + msi + "\"" // Specify the .msi to install
    + " /qb+!" // Basic UI with modal dialog at end, but without [Cancel] button
    + " /l*vx \"" + logfile.string() + "\"" // Log all information, including verbose output and extra debugging information
    + " UPGRADEFROMASSESSOR=true"; // Let the installer know it's being invoked from pepAssessor.exe

  auto prompt = true;
#ifndef PEP_INSTALLER_ALWAYS_PROMPT_FOR_CREDENTIALS
  prompt = pep::win32api::GetElevationState() == pep::win32api::CANNOT_ELEVATE;
#endif
  if (prompt) {
    // Use helper (bootstrapper) .EXE to elevate a different account: see https://techcommunity.microsoft.com/t5/Windows-Blog-Archive/Why-Can-8217-t-I-Elevate-My-Application-to-Run-As-Administrator/ba-p/228626
    context.getAdministrativeCredentials().runCommandLine("\"" + context.elevateExe.string() + "\" " + cmd + " " + parameters);
  }
  else {
    parameters += " UPGRADEFROMASSESSORACCOUNT=true"; // Let the installer know it's being invoked from the assessor account
    pep::win32api::StartProcess(cmd, parameters, true);
  }
}

pep::SemanticVersion Installer::getSemver() const {
  return pep::SemanticVersion(getMajorVersion(), getMinorVersion(), getBuild(), getRevision());
}

std::shared_ptr<Installer> Installer::GetAvailable() {
#ifdef PEP_INSTALLER_BROWSE_LOCALLY
  return LocalInstaller::GetAvailable();
#else
  return PublishedInstaller::GetAvailable();
#endif
}


#endif /*_WIN32*/
