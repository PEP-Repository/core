#include <pep/castor/Ptree.hpp>

#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <entities.hpp>

namespace pep {
namespace castor {

namespace detail {

PtreeAccess<std::string>::ReturnType PtreeAccess<std::string>::Get(const boost::property_tree::ptree& ptree, const std::string& path) {
  return decode_html_entities_utf8(ptree.get<std::string>(path));
}

PtreeAccess<boost::property_tree::ptree>::ReturnType PtreeAccess<boost::property_tree::ptree>::GetImpl(const boost::property_tree::ptree& parent, const std::string& path) {
  try {
    return parent.get_child(path);
  }
  catch (...) {
    std::ostringstream json;
    boost::property_tree::write_json(json, parent);
    LOG("Castor", error) << "Error \"" << GetExceptionMessage(std::current_exception())
      << "\" occurred attempting to find path \"" << path << "\" in the property tree with the following JSON representation:\n"
      << json.str();
    throw;
  }
}

PtreeAccess<boost::optional<boost::property_tree::ptree>>::ReturnType PtreeAccess<boost::optional<boost::property_tree::ptree>>::GetImpl(const boost::property_tree::ptree& parent, const std::string& path) {
  auto child = parent.get_child_optional(path);
  if (child->empty() && child->data() == "null") {
    return boost::none;
  }
  return child;
}

}

void ReadJsonIntoPtree(boost::property_tree::ptree& destination, const std::string& source) {
  try {
    std::istringstream stream(source);
    boost::property_tree::read_json(stream, destination);
  }
  catch (...) {
    LOG("Castor", error) << "Error \"" << GetExceptionMessage(std::current_exception())
      << "\" occurred attempting to read the following data as JSON:\n"
      << source;
    throw;
  }
}

std::string PtreeToJson(const boost::property_tree::ptree& ptree) {
  std::ostringstream result;
  boost::property_tree::write_json(result, ptree);
  return std::move(result).str();
}

}
}
