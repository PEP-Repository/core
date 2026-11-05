#pragma once

#include <pep/castor/CastorConnection.hpp> // for castor::JsonPtr
#include <pep/castor/CastorObject.hpp> // for castor::SimpleCastorChildObject
#include <pep/castor/Study.hpp>
#include <pep/utils/Shared.hpp>

#include <map>
#include <memory>
#include <string>

namespace pep {
namespace castor {

class OptionGroup : public SimpleCastorChildObject<OptionGroup, Study>, public SharedConstructor<OptionGroup> {
 private:
  std::string name_;
  std::map<std::string, std::string> options_;

 public:
  static const std::string RelativeApiEndpoint;
  static const std::string EmbeddedApiNodeName;

  std::string getName() const { return name_; }

  const std::map<std::string, std::string>& getOptions() const { return options_; }

 protected:
  OptionGroup(std::shared_ptr<Study> study, JsonPtr json);

 private:
  friend class SharedConstructor<OptionGroup>;
};

}
}
