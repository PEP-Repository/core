#include <pep/networking/HttpMethod.hpp>
#include <boost/bimap.hpp>

namespace pep::networking {

namespace {

const boost::bimap<HttpMethod::Value, std::string> HTTP_METHOD_STRING_MAPPINGS = []() {
  boost::bimap<HttpMethod::Value, std::string> result;
  result.insert({ HttpMethod::GET, "GET" });
  result.insert({ HttpMethod::POST, "POST" });
  result.insert({ HttpMethod::PUT, "PUT" });
  return result;
  }();

std::string HttpMethodIdentifierToString(HttpMethod::Value id) {
  return std::to_string(id);
}

std::string HttpMethodIdentifierToString(const std::string& id) {
  return id;
}

template <typename TView, typename TValue>
auto GetHttpMethodMappedValue(const TView& mapSide, const TValue& value) {
  auto pos = mapSide.find(value);
  if (pos == mapSide.end()) {
    throw std::runtime_error("Unsupported HTTP method identifier " + HttpMethodIdentifierToString(value));
  }
  return pos->second;
}

}

std::string HttpMethod::toString() const {
  return GetHttpMethodMappedValue(HTTP_METHOD_STRING_MAPPINGS.left, mValue);
}

HttpMethod HttpMethod::FromString(const std::string& str) {
  return GetHttpMethodMappedValue(HTTP_METHOD_STRING_MAPPINGS.right, str);
}

}
