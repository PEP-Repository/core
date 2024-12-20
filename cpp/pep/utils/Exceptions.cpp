#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <boost/system/error_code.hpp>
#include <boost/core/demangle.hpp>
#include <boost/exception/all.hpp>
#include <boost/system/system_error.hpp>
#include <boost/config.hpp>

#ifndef _WIN32
// Assume that cxxabi is available on *nix/Mac
#include <cxxabi.h>
#endif

#include <exception>
#include <typeinfo>
#include <utility>

namespace pep {

namespace {

std::string GetCurrentExceptionTypeName() {
#ifndef _WIN32
  const std::type_info* currentExceptionType = abi::__cxa_current_exception_type();
  assert(currentExceptionType && "No caught exception");
  return boost::core::demangle(currentExceptionType->name());
#else
  return "[unknown exception type]";
#endif
}

template<typename T>
std::string GetCurrentExceptionTypeName([[maybe_unused]] const T& ex) {
#ifndef BOOST_NO_RTTI
  return boost::core::demangle(typeid(ex).name());
#endif

  return GetCurrentExceptionTypeName();
}

} // namespace

std::string GetExceptionMessage(std::exception_ptr source) {
  // Can't determine a type name if there's no exception(ptr)
  if (source == nullptr) {
    return "[null std::exception_ptr]";
  }

  try {
    std::rethrow_exception(std::move(source));
  } catch (const boost::exception& e) {
    return boost::diagnostic_information(e);
  } catch (const boost::system::system_error& error) {
    std::ostringstream message;
    message << "System error: " << error.code() << " - " << error.what() << " - " << error.code().message();
    return message.str();
  } catch (const std::exception& e) {
    return GetCurrentExceptionTypeName(e) + ": " + e.what();
  } catch (const boost::system::error_code& e) {
    return GetCurrentExceptionTypeName(e) + ": " + e.message();
  } catch ( ... ) {
    return GetCurrentExceptionTypeName();
  }

}

}
