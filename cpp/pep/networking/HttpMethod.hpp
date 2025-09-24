#pragma once

#include <ostream>
#include <string>

namespace pep::networking {

/**
 * @brief Represents an HTTP method.
 * @remark Encapsulates conversion to and from string, and ostream formatting (as string).
 */
class HttpMethod {
public:
  /**
   * @brief An enum representing the supported method(type)s.
   */
  enum Value {
    GET,
    POST,
    PUT
  };

  /**
   * @brief Constructor.
   * @param value The method(type) that this instance will represent.
   */
  HttpMethod(Value value) noexcept : mValue(value) {} // Implicitly convertible

  /**
   * @brief Produces the method(type) that this instance represents.
   */
  Value value() const noexcept { return mValue; }

  /**
   * @brief Produces the (HTTP compliant) string representation of this method(type).
   * @return A string representation of the method(type).
   */
  std::string toString() const;

  /**
   * @brief Converts a(n HTTP compliant) string representation to an HttpMethod.
   * @return An HttpMethod instance representing the specified HTTP method string.
   * @remark Parsing method.
   */
  static HttpMethod FromString(const std::string& str);

  /**
   * @brief Ordinal comparison.
   * @remark Allows HttpMethod instances to be used as keys in keyed containers (that are based on ordinal comparison, as opposed to hashing).
   */
  auto operator<=>(const HttpMethod&) const = default;

private:
  Value mValue;
};

/**
 * @brief Stream output operator for HttpMethod instances.
 * @param lhs The stream to which to write the HttpMethod.
 * @param rhs The HttpMethod to write to the stream.
 * @return The stream.
 * @remark Writes the HttpMethod's string representation to the stream.
 */
inline std::ostream& operator<<(std::ostream& lhs, const HttpMethod& rhs) { return lhs << rhs.toString(); }

}
