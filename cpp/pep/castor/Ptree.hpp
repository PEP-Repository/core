#pragma once

#include <optional>

#include <boost/property_tree/ptree.hpp>

namespace pep {
namespace castor {

namespace detail {

/*! \brief Helper struct for the GetFromPtree<> function.
 *  \remark Partial specializations provide specialized processing for specific value types.
 */
template <typename T>
struct PtreeAccess {
  using ReturnType = T;

  static ReturnType Get(const boost::property_tree::ptree& ptree, const std::string& path) {
    return ptree.get<T>(path);
  }
};

template <>
struct PtreeAccess<std::string> {
  using ReturnType = std::string;

  static ReturnType Get(const boost::property_tree::ptree& ptree, const std::string& path);
};

template <>
struct PtreeAccess<boost::property_tree::ptree> {
  using ReturnType = const boost::property_tree::ptree&;

private:
  static ReturnType GetImpl(const boost::property_tree::ptree& parent, const std::string& path);

public:
  static ReturnType Get(std::convertible_to<boost::property_tree::ptree> auto&& parent, const std::string& path) {
    static_assert(std::is_lvalue_reference_v<decltype(parent)>,
      "Don't pass in an rvalue here, as the returned ptree ref will be dangling");
    return GetImpl(parent, path);
  }
};

template <typename T>
struct PtreeAccess<std::optional<T>> {
  using ReturnType = std::optional<T>;

  static ReturnType Get(const boost::property_tree::ptree& ptree, const std::string& path) {
    auto result = PtreeAccess<boost::optional<T>>::Get(ptree, path);
    if (result) {
      return *result;
    }
    return std::nullopt;
  }
};

template <>
struct PtreeAccess<boost::optional<boost::property_tree::ptree>> {
  using ReturnType = boost::optional<const boost::property_tree::ptree&>;

private:
  static ReturnType GetImpl(const boost::property_tree::ptree& parent, const std::string& path);

public:
  static ReturnType Get(std::convertible_to<boost::property_tree::ptree> auto&& parent, const std::string& path) {
    static_assert(std::is_lvalue_reference_v<decltype(parent)>,
      "Don't pass in an rvalue here, as the returned ptree ref will be dangling");
    return GetImpl(parent, path);
  }
};

template <>
struct PtreeAccess<std::optional<boost::property_tree::ptree>> {
  // Specialization exists to prevent this case from being handled by other specializations, but
  // this access type is not supported because we can't create an std::optional<ptree&> .
};

}

/*! \brief Reads JSON data into a Boost property tree.
 * \param destination The property tree to fill
 * \param source The JSON text to read
 * \remark Log-producing replacement for boost::property_tree::read_json.
 */
void ReadJsonIntoPtree(boost::property_tree::ptree& destination, const std::string& source);

/*! \brief Converts a Boost property tree to a (pretty-printed) JSON string.
 * \param ptree The property tree to convert
 * \return The JSON representation of the parameter.
 * \remark Usable as a debug helper: invoke pep::castor::PtreeToJson on your ptree to get a readable rendering.
 */
std::string PtreeToJson(const boost::property_tree::ptree& ptree);

/*! \brief Reads a value from a property tree.
 * \tparam T The type of value to read. Use std::optional<MyType> to read optional nodes; use boost::property_tree::ptree to read child trees.
 * \param ptree The property tree
 * \param path The path to the value
 * \return The value
 * \remark Child trees are returned by (const) reference for efficiency. Other types are returned by value.
 */
#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdangling-reference" // We guard against most dangling ptree references above
#endif
template <typename T>
typename detail::PtreeAccess<T>::ReturnType GetFromPtree(std::convertible_to<boost::property_tree::ptree> auto&& ptree, const std::string& path) {
  // Since functions cannot be partially specialized (e.g. for std::optional<T> values), we just invoke a method on a helper struct which *is* partially specialized.
  return detail::PtreeAccess<T>::Get(std::forward<decltype(ptree)>(ptree), path);
}
#if defined(__GNUC__) && !defined(__clang__)
# pragma GCC diagnostic pop
#endif

}
}
