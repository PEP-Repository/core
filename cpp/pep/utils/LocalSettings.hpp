#pragma once

#include <boost/property_tree/ptree.hpp>
#include <string>

namespace pep {

/* LocalSettings allows for non-volatile storage and retrieval of settings.
 * There is a global instance which can be accessed through getInstance().
 * Data is accessible on the local machine for the currently logged-on user.
 * On unix, the values are stored in a INI file in the user's home directory.
 * On windows, the windows registry is used (within the user's scope).
 *
 * Changes are not permanently stored until flushChanges() is called.
 */

class LocalSettings {
 public:
  LocalSettings() = default;
  virtual ~LocalSettings () = default;
  LocalSettings (const LocalSettings &other) = default;
  LocalSettings (LocalSettings &&other) = default;
  LocalSettings& operator=(const LocalSettings& other) = default;
  LocalSettings& operator=(LocalSettings&& other) = default;

  virtual bool storeValue (const std::string& szNamespace, const std::string& szPropertyName, const std::string& szValue);
  virtual bool storeValue (const std::string& szNamespace, const std::string& szPropertyName, int dwValue);
  bool retrieveValue (std::string* lpValue, const std::string& szNamespace, const std::string& szPropertyName) const;
  bool retrieveValue (int* lpValue, const std::string& szNamespace, const std::string& szPropertyName) const;
  virtual bool deleteValue (const std::string& szNamespace, const std::string& szPropertyName);
  virtual bool flushChanges ();

  static std::unique_ptr<LocalSettings>& getInstance ();

  protected:
    boost::property_tree::ptree mPropertyTree;
};
}
