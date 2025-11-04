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

static const std::string LOG_TAG ("LocalSettings");

bool LocalSettings::retrieveValue(
  std::string* lpValue,
  const std::string& szNamespace,
  const std::string& szPropertyName
) const {
  if (auto value = mPropertyTree.get_optional<std::string>(szNamespace + "." + szPropertyName)) {
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
  mPropertyTree.put (szNamespace + "." + szPropertyName, szValue);
  return true;
}

bool LocalSettings::storeValue(
  const std::string& szNamespace,
  const std::string& szPropertyName,
  int dwValue
) {
  mPropertyTree.put (szNamespace + "." + szPropertyName, std::to_string(dwValue));
  return true;
}

bool LocalSettings::deleteValue (const std::string& szNamespace, const std::string& szPropertyName) {
  if (auto nodeRef = mPropertyTree.get_child_optional(szNamespace)) {
    const auto numErasedChildren = nodeRef->erase(szPropertyName);
    return numErasedChildren > 0;
  }
  return false;
}

bool LocalSettings::flushChanges() {
  throw std::runtime_error("LocalSettings::flushChanges called without persistent storage implementation");
}


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
    ::RegCloseKey(mRegistryHandle);
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
    HKEY mRegistrySubKey
  );
  /* StoreRecursive stores the modifications to the internal tree into the registry.
   * it takes the actual property tree and the modification bookkeeping
   * in order to update only the modified values and nothing more.
   * Recursive, as with RetrieveRecursive.
   */
  static void StoreRecursive (
    HKEY mRegistrySubKey,
    boost::property_tree::ptree& mPropertySubtree,
    boost::property_tree::ptree& mModifiedSubtree
  );
  /* DeleteRecursive deletes values from registry that have been flagged deleted
   * in the internal boost property tree.
   */
  static void DeleteRecursive(
    HKEY mRegistrySubKey,
    boost::property_tree::ptree& mDeletedSubtree
  );
  /* setModifiedFlag flags the given property so that it will be updated when flushChanges() is called.
   * Additionally, in case the property was flagged deleted before, the flag is removed
   */
  void setModifiedFlag(const std::string& szNamespace, const std::string& szPropertyName);

  HKEY mRegistryHandle;
  std::string szSubKeyName;
  boost::property_tree::ptree mDeletedValues;
  boost::property_tree::ptree mModifiedValues;
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
  mModifiedValues.put(szNamespace + "." + szPropertyName, "");
}

bool LocalSettingsRegistry::deleteValue (const std::string& szNamespace, const std::string& szPropertyName) {
  LocalSettings::deleteValue(szNamespace, szPropertyName);
  boost::property_tree::ptree mNamespace;

  mDeletedValues.put(szNamespace + "." + szPropertyName, "");
  try {
    mNamespace = mModifiedValues.get_child(szNamespace);
    mNamespace.erase(szPropertyName);
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
        &mRegistryHandle,
        nullptr
      ) != ERROR_SUCCESS) {
    return;
  }

  mPropertyTree = RetrieveRecursive (mRegistryHandle);
}

bool LocalSettingsRegistry::flushChanges () {
  DeleteRecursive (mRegistryHandle, mDeletedValues);
  StoreRecursive (mRegistryHandle, mPropertyTree, mModifiedValues);
  mDeletedValues.clear();
  mModifiedValues.clear();
  return true;
}

boost::property_tree::ptree LocalSettingsRegistry::RetrieveRecursive (
  HKEY mRegistrySubKey
) {
  DWORD dwIndex;
  DWORD dwType;
  boost::property_tree::ptree mResult;
  char abKeyBuffer[256];
  BYTE abValueBuffer[256];
  DWORD dwKeyLength;
  DWORD dwValueLength;
  HKEY mChildSubKey;

  for(dwIndex = 0; ; dwIndex ++) {
    dwKeyLength = 256;

    if (::RegEnumKeyExA (
          mRegistrySubKey,
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
          mRegistrySubKey,
          abKeyBuffer,
          0,
          KEY_READ,
          &mChildSubKey
        ) == ERROR_SUCCESS) {
      mResult.put_child (abKeyBuffer, RetrieveRecursive(mChildSubKey));
      RegCloseKey (mChildSubKey);
    }
  }

  for (dwIndex = 0; ; dwIndex ++) {
    dwKeyLength = 256;

    if (::RegEnumValueA (
          mRegistrySubKey,
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
          mRegistrySubKey,
          abKeyBuffer,
          0,
          &dwType,
          abValueBuffer,
          &dwValueLength
        ) == ERROR_SUCCESS) {
      switch (dwType) {
      case REG_SZ:
        mResult.put (abKeyBuffer, std::string(reinterpret_cast<char*>(abValueBuffer), dwValueLength - 1));
        break;
      case REG_BINARY:
        mResult.put (abKeyBuffer, std::string(reinterpret_cast<char*>(abValueBuffer), dwValueLength));
        break;
      case REG_DWORD:
        mResult.put (abKeyBuffer, std::to_string(*reinterpret_cast<uint32_t*>(abValueBuffer)));
        break;
      case REG_QWORD:
        mResult.put (abKeyBuffer, std::to_string(*reinterpret_cast<uint64_t*>(abValueBuffer)));
        break;
      }
    }
  }

  return mResult;
}

void LocalSettingsRegistry::DeleteRecursive(
  HKEY mRegistrySubKey,
  boost::property_tree::ptree& mDeletedSubtree
) {
  boost::property_tree::ptree::iterator it;

  for (it = mDeletedSubtree.begin(); it != mDeletedSubtree.end(); it++) {
    HKEY mChildSubKey;
    if (it->second.empty()) {
      if (::RegDeleteValue(
            mRegistrySubKey,
            it->first.c_str()
          ) != ERROR_SUCCESS) {
        //::RegDeleteKey(
        //  mRegistrySubKey,
        //  it->first.c_str()
        //);
      }
    } else {
      if (::RegOpenKeyExA(
            mRegistrySubKey,
            it->first.c_str(),
            0,
            KEY_READ | KEY_WRITE,
            &mChildSubKey
          ) != ERROR_SUCCESS) {
        continue;
      }
      DeleteRecursive(mChildSubKey, it->second);
      ::RegCloseKey(mChildSubKey);
    }
  }
}

void LocalSettingsRegistry::StoreRecursive (
  HKEY mRegistrySubKey,
  boost::property_tree::ptree& mPropertySubtree,
  boost::property_tree::ptree& mModifiedSubtree
) {
  boost::property_tree::ptree::iterator it;

  for (it = mModifiedSubtree.begin(); it != mModifiedSubtree.end(); it++) {
    HKEY mChildSubKey;
    boost::property_tree::ptree mChildPropertySubtree;

    if (it->second.empty()) {
      /* empty means leaf node -- node is modified */

      auto nodeRef = mPropertySubtree.get_child_optional(it->first);
      if (!nodeRef) {
        LOG(LOG_TAG, debug) << "Unable to find entry " << it->first << " in value subtree";
        continue;
      }
      const auto& mValueNode = *nodeRef;

      if (!mValueNode.empty()) {
        /* mValueNode should always contain values, not subtrees */
        LOG(LOG_TAG, debug) << "Entry " << it->first << " in value subtree is not a leaf";
        continue;
      }

      ::RegSetValueExA(
        mRegistrySubKey,
        it->first.c_str(),
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(mValueNode.data().c_str()),
        static_cast<DWORD>(mValueNode.data().length())
      );
    } else {
      /* internal node -- create reg key if not already exists */
      if (::RegCreateKeyExA(
            mRegistrySubKey,
            it->first.c_str(),
            0,
            nullptr,
            0,
            KEY_READ | KEY_WRITE,
            nullptr,
            &mChildSubKey,
            nullptr
          ) != ERROR_SUCCESS) {
        continue;
      }

      try {
        mChildPropertySubtree = mPropertySubtree.get_child(it->first);
        StoreRecursive(mChildSubKey, mChildPropertySubtree, it->second);
      } catch (const boost::property_tree::ptree_bad_path&) {
        LOG(LOG_TAG, debug) << "Entry " << it->first << " is flagged modified but not found in property tree";
      }

      ::RegCloseKey(mChildSubKey);
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
  std::string szFilename;
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
        if (!err) {
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
    this->szFilename = std::string(homeDir) + "/.pep/LocalSettings.ini";
  } else {
    this->szFilename = szFilename;
  }

  // Can we find & open the file?
  if (std::ifstream iniStream{this->szFilename}) {
    read_ini(iniStream, mPropertyTree);
  }
}


bool LocalSettingsIni::flushChanges() {
  const auto dir = std::filesystem::path(this->szFilename).parent_path();
  create_directory(dir);
  permissions(dir, std::filesystem::perms::owner_all);

  try {
    boost::property_tree::write_ini (this->szFilename, mPropertyTree);
  } catch (const std::exception& e) {
    LOG(LOG_TAG, debug) << "Unable to write ini : " << e.what();
    return false;
  }
  return true;
}

#endif

std::unique_ptr<LocalSettings>& LocalSettings::getInstance() {
#ifdef _WIN32
  static std::unique_ptr<LocalSettings> instance = std::make_unique<LocalSettingsRegistry>();
#else
  static std::unique_ptr<LocalSettings> instance = std::make_unique<LocalSettingsIni>();
#endif

  return instance;
}

} // namespace pep
