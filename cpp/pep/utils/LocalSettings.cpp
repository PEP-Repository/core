#include <pep/utils/LocalSettings.hpp>
#include <pep/utils/Platform.hpp>
#include <pep/utils/Log.hpp>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <utility>
#include <sys/stat.h>

#include <boost/property_tree/ptree.hpp>
#ifndef _WIN32
#include <boost/property_tree/ini_parser.hpp>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace pep {

namespace {

const std::string LogTag("LocalSettings");

}

bool LocalSettings::retrieveValue(
  std::string* lpValue,
  const std::string& szNamespace,
  const std::string& szPropertyName
) const {
  if (auto value = propertyTree_.get_optional<std::string>(szNamespace + "." + szPropertyName)) {
    *lpValue = std::move(*value);
    return true;
  }
  return false;
}

bool LocalSettings::retrieveValue(
  int* dwValue,
  const std::string& szNamespace,
  const std::string& szPropertyName
) const {
  std::string szValue;
  if (!retrieveValue (&szValue, szNamespace, szPropertyName)) {
    return false;
  }

  try {
    *dwValue = std::stoi(szValue);
  } catch (const std::logic_error&) {
    return false;
  }

  return true;
}

bool LocalSettings::storeValue(
  const std::string& szNamespace,
  const std::string& szPropertyName,
  const std::string& szValue
) {
  propertyTree_.put (szNamespace + "." + szPropertyName, szValue);
  return true;
}

bool LocalSettings::storeValue(
  const std::string& szNamespace,
  const std::string& szPropertyName,
  int dwValue
) {
  propertyTree_.put (szNamespace + "." + szPropertyName, std::to_string(dwValue));
  return true;
}

bool LocalSettings::deleteValue (const std::string& szNamespace, const std::string& szPropertyName) {
  if (auto nodeRef = propertyTree_.get_child_optional(szNamespace)) {
    const auto numErasedChildren = nodeRef->erase(szPropertyName);
    return numErasedChildren > 0;
  }
  return false;
}

bool LocalSettings::flushChanges() {
  throw std::runtime_error("LocalSettings::flushChanges called without persistent storage implementation");
}


namespace {
#ifdef _WIN32
/* LocalSettingsRegistry is the storage backend used on windows.
 * In order to avoid deleting and rebuilding the entire tree in the windows registry
 * for every call to flushChanges(), modifications and deletions to the tree are tracked
 * by overriding storeValue and deleteValue.
 */

class LocalSettingsRegistry: public LocalSettings {
 public:
  explicit LocalSettingsRegistry (const std::string& szSubKeyName = "");
  ~LocalSettingsRegistry() override {
    ::RegCloseKey(registryHandle_);
  }

  /* LocalSettings::storeValue, with bookkeeping of what settings are modified */
  bool storeValue(const std::string& szNamespace, const std::string& szPropertyName, const std::string& szValue) override;
  bool storeValue(const std::string& szNamespace, const std::string& szPropertyName, int dwValue) override;
  /* LocalSettings::deleteValue, with bookkeeping */
  bool deleteValue (const std::string& szNamespace, const std::string& szPropertyName) override;
  bool flushChanges () override;

 protected:
  /* RetrieveRecursive builds a boost property tree from the registry.
   * Recursive in order to be depth agnostic (although currently only 2 levels are used by LocalSettings).
   */
  static boost::property_tree::ptree RetrieveRecursive (
    HKEY registrySubKey_
  );
  /* StoreRecursive stores the modifications to the internal tree into the registry.
   * it takes the actual property tree and the modification bookkeeping
   * in order to update only the modified values and nothing more.
   * Recursive, as with RetrieveRecursive.
   */
  static void StoreRecursive (
    HKEY registrySubKey_,
    boost::property_tree::ptree& propertySubtree_,
    boost::property_tree::ptree& modifiedSubtree_
  );
  /* DeleteRecursive deletes values from registry that have been flagged deleted
   * in the internal boost property tree.
   */
  static void DeleteRecursive(
    HKEY registrySubKey_,
    boost::property_tree::ptree& deletedSubtree_
  );
  /* setModifiedFlag flags the given property so that it will be updated when flushChanges() is called.
   * Additionally, in case the property was flagged deleted before, the flag is removed
   */
  void setModifiedFlag(const std::string& szNamespace, const std::string& szPropertyName);

  HKEY registryHandle_;
  std::string szSubKeyName;
  boost::property_tree::ptree deletedValues_;
  boost::property_tree::ptree modifiedValues_;
};

bool LocalSettingsRegistry::storeValue(
  const std::string& szNamespace,
  const std::string& szPropertyName,
  const std::string& szValue
) {
  bool bResult;
  if ((bResult = LocalSettings::storeValue(szNamespace, szPropertyName, szValue)) != false) {
    setModifiedFlag (szNamespace, szPropertyName);
  }
  return bResult;
}

bool LocalSettingsRegistry::storeValue(
  const std::string& szNamespace,
  const std::string& szPropertyName,
  int dwValue
) {
  bool bResult;
  if ((bResult = LocalSettings::storeValue(szNamespace, szPropertyName, dwValue)) != false) {
    setModifiedFlag(szNamespace, szPropertyName);
  }
  return bResult;
}

void LocalSettingsRegistry::setModifiedFlag (const std::string& szNamespace, const std::string& szPropertyName) {
  modifiedValues_.put(szNamespace + "." + szPropertyName, "");
}

bool LocalSettingsRegistry::deleteValue (const std::string& szNamespace, const std::string& szPropertyName) {
  LocalSettings::deleteValue(szNamespace, szPropertyName);
  boost::property_tree::ptree namespace_;

  deletedValues_.put(szNamespace + "." + szPropertyName, "");
  try {
    namespace_ = modifiedValues_.get_child(szNamespace);
    namespace_.erase(szPropertyName);
  } catch (const boost::property_tree::ptree_bad_path&) { }
  return true;
}

LocalSettingsRegistry::LocalSettingsRegistry (const std::string& szSubKeyName) {
  if (szSubKeyName.empty()) {
    this->szSubKeyName = "Software\\PEP\\LocalSettings";
  } else {
    this->szSubKeyName = szSubKeyName;
  }

  if (::RegCreateKeyExA (
        HKEY_CURRENT_USER,
        this->szSubKeyName.c_str(),
        0,
        nullptr,
        0,
        KEY_READ | KEY_WRITE,
        nullptr,
        &registryHandle_,
        nullptr
      ) != ERROR_SUCCESS) {
    return;
  }

  propertyTree_ = RetrieveRecursive (registryHandle_);
}

bool LocalSettingsRegistry::flushChanges () {
  DeleteRecursive (registryHandle_, deletedValues_);
  StoreRecursive (registryHandle_, propertyTree_, modifiedValues_);
  deletedValues_.clear();
  modifiedValues_.clear();
  return true;
}

boost::property_tree::ptree LocalSettingsRegistry::RetrieveRecursive (
  HKEY registrySubKey_
) {
  DWORD dwIndex;
  DWORD dwType;
  boost::property_tree::ptree result_;
  char abKeyBuffer[256];
  BYTE abValueBuffer[256];
  DWORD dwKeyLength;
  DWORD dwValueLength;
  HKEY childSubKey_;

  for(dwIndex = 0; ; dwIndex ++) {
    dwKeyLength = 256;

    if (::RegEnumKeyExA (
          registrySubKey_,
          dwIndex,
          abKeyBuffer,
          &dwKeyLength,
          nullptr,
          nullptr,
          nullptr,
          nullptr
        ) != ERROR_SUCCESS) {
      break;
    }

    if (::RegOpenKeyExA (
          registrySubKey_,
          abKeyBuffer,
          0,
          KEY_READ,
          &childSubKey_
        ) == ERROR_SUCCESS) {
      result_.put_child (abKeyBuffer, RetrieveRecursive(childSubKey_));
      RegCloseKey (childSubKey_);
    }
  }

  for (dwIndex = 0; ; dwIndex ++) {
    dwKeyLength = 256;

    if (::RegEnumValueA (
          registrySubKey_,
          dwIndex,
          abKeyBuffer,
          &dwKeyLength,
          nullptr,
          nullptr,
          nullptr,
          nullptr
        ) != ERROR_SUCCESS) {
      break;
    }

    dwValueLength = 256;

    if (::RegQueryValueExA (
          registrySubKey_,
          abKeyBuffer,
          0,
          &dwType,
          abValueBuffer,
          &dwValueLength
        ) == ERROR_SUCCESS) {
      switch (dwType) {
      case REG_SZ:
        result_.put (abKeyBuffer, std::string(reinterpret_cast<char*>(abValueBuffer), dwValueLength - 1));
        break;
      case REG_BINARY:
        result_.put (abKeyBuffer, std::string(reinterpret_cast<char*>(abValueBuffer), dwValueLength));
        break;
      case REG_DWORD:
        result_.put (abKeyBuffer, std::to_string(*reinterpret_cast<uint32_t*>(abValueBuffer)));
        break;
      case REG_QWORD:
        result_.put (abKeyBuffer, std::to_string(*reinterpret_cast<uint64_t*>(abValueBuffer)));
        break;
      }
    }
  }

  return result_;
}

void LocalSettingsRegistry::DeleteRecursive(
  HKEY registrySubKey_,
  boost::property_tree::ptree& deletedSubtree_
) {
  boost::property_tree::ptree::iterator it;

  for (it = deletedSubtree_.begin(); it != deletedSubtree_.end(); it++) {
    HKEY childSubKey_;
    if (it->second.empty()) {
      if (::RegDeleteValue(
            registrySubKey_,
            it->first.c_str()
          ) != ERROR_SUCCESS) {
        //::RegDeleteKey(
        //  registrySubKey_,
        //  it->first.c_str()
        //);
      }
    } else {
      if (::RegOpenKeyExA(
            registrySubKey_,
            it->first.c_str(),
            0,
            KEY_READ | KEY_WRITE,
            &childSubKey_
          ) != ERROR_SUCCESS) {
        continue;
      }
      DeleteRecursive(childSubKey_, it->second);
      ::RegCloseKey(childSubKey_);
    }
  }
}

void LocalSettingsRegistry::StoreRecursive (
  HKEY registrySubKey_,
  boost::property_tree::ptree& propertySubtree_,
  boost::property_tree::ptree& modifiedSubtree_
) {
  boost::property_tree::ptree::iterator it;

  for (it = modifiedSubtree_.begin(); it != modifiedSubtree_.end(); it++) {
    HKEY childSubKey_;
    boost::property_tree::ptree childPropertySubtree_;

    if (it->second.empty()) {
      /* empty means leaf node -- node is modified */

      auto nodeRef = propertySubtree_.get_child_optional(it->first);
      if (!nodeRef) {
        PEP_LOG(LogTag, Severity::Debug) << "Unable to find entry " << it->first << " in value subtree";
        continue;
      }
      const auto& valueNode_ = *nodeRef;

      if (!valueNode_.empty()) {
        /* valueNode_ should always contain values, not subtrees */
        PEP_LOG(LogTag, Severity::Debug) << "Entry " << it->first << " in value subtree is not a leaf";
        continue;
      }

      ::RegSetValueExA(
        registrySubKey_,
        it->first.c_str(),
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(valueNode_.data().c_str()),
        static_cast<DWORD>(valueNode_.data().length())
      );
    } else {
      /* internal node -- create reg key if not already exists */
      if (::RegCreateKeyExA(
            registrySubKey_,
            it->first.c_str(),
            0,
            nullptr,
            0,
            KEY_READ | KEY_WRITE,
            nullptr,
            &childSubKey_,
            nullptr
          ) != ERROR_SUCCESS) {
        continue;
      }

      try {
        childPropertySubtree_ = propertySubtree_.get_child(it->first);
        StoreRecursive(childSubKey_, childPropertySubtree_, it->second);
      } catch (const boost::property_tree::ptree_bad_path&) {
        PEP_LOG(LogTag, Severity::Debug) << "Entry " << it->first << " is flagged modified but not found in property tree";
      }

      ::RegCloseKey(childSubKey_);
    }
  }
}
#else
/* LocalSettingsIni is the storage backend used on unix.
 * The INI file is stored in the user's home directory and re-generated
 * for every call to flushChanges().
 */

class LocalSettingsIni : public LocalSettings {
 public:
  explicit LocalSettingsIni (const std::string& szFilename = "");
  bool flushChanges () override;

 private:
  std::string szFilename_;
};

LocalSettingsIni::LocalSettingsIni (const std::string& szFilename) {
  if (szFilename.empty()) {
    //NOLINTNEXTLINE(concurrency-mt-unsafe) std::getenv is thread safe as long as we do not setenv/unsetenv/putenv
    char* homeDir = std::getenv("HOME");
    if(!homeDir) {
      size_t initBufSize{1024};
      if (auto ret = sysconf(_SC_GETPW_R_SIZE_MAX); ret != -1) {
        initBufSize = static_cast<size_t>(ret);
      }
      std::vector<char> buf(initBufSize);
      passwd pwdBuf{}, *result{};
      while (true) {
        auto err = getpwuid_r(getuid(), &pwdBuf, buf.data(), buf.size(), &result);
        if (err == 0) {
          break;
        }
        if (err != ERANGE) {
          throw std::system_error(err, std::generic_category());
        }
        buf.resize(buf.size() * 2);
      }
      assert(result);
      homeDir = result->pw_dir;
    }
    szFilename_ = std::string(homeDir) + "/.pep/LocalSettings.ini";
  } else {
    szFilename_ = szFilename;
  }

  // Can we find & open the file?
  if (std::ifstream iniStream{szFilename_}) {
    read_ini(iniStream, propertyTree_);
  }
}


bool LocalSettingsIni::flushChanges() {
  const auto dir = std::filesystem::path(szFilename_).parent_path();
  create_directory(dir);
  permissions(dir, std::filesystem::perms::owner_all);

  try {
    boost::property_tree::write_ini (szFilename_, propertyTree_);
  } catch (const std::exception& e) {
    PEP_LOG(LogTag, Severity::Debug) << "Unable to write ini : " << e.what();
    return false;
  }
  return true;
}

#endif
} // namespace

std::unique_ptr<LocalSettings>& LocalSettings::getInstance() {
#ifdef _WIN32
  static std::unique_ptr<LocalSettings> instance = std::make_unique<LocalSettingsRegistry>();
#else
  static std::unique_ptr<LocalSettings> instance = std::make_unique<LocalSettingsIni>();
#endif

  return instance;
}

} // namespace pep
