#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class OptionGroup : public SimpleCastorChildObject<OptionGroup, Study>, public SharedConstructor<OptionGroup> {
 private:
  std::string mName;
  std::map<std::string, std::string> mOptions;

 public:
  static const std::string RELATIVE_API_ENDPOINT;
  static const std::string EMBEDDED_API_NODE_NAME;

  std::string getName() const { return mName; }

  const std::map<std::string, std::string>& getOptions() const { return mOptions; }

 protected:
  OptionGroup(std::shared_ptr<Study> study, JsonPtr json);

 private:
  friend class SharedConstructor<OptionGroup>;
};

}
}
