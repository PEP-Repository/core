#include <pep/weblib/ThreadPrintable.hpp>

#include <pep/utils/Defer.hpp>

#include <emscripten/threading.h>
#include <emscripten/val.h>

std::ostream& pep::weblib::operator<<(std::ostream& os, const CurrentThreadPrintable&) {
  const auto flags = os.flags();
  PEP_DEFER(os.flags(flags));
  os << std::hex << "0x" << ::pthread_self();
  if (::emscripten_is_main_runtime_thread()) {
    os << " main";
  } else {
    emscripten::val name = emscripten::val::global("name");
    if (name.isString()) {
      os << ' ' << name.as<std::string>();
    }
  }
  return os;
}

std::ostream& pep::weblib::operator<<(std::ostream& os, const ThreadPrintable& self) {
  const auto flags = os.flags();
  PEP_DEFER(os.flags(flags));
  os << std::hex << "0x" << self.thread_;
  if (self.thread_ == ::emscripten_main_runtime_thread_id()) {
    os << " (main)";
  }
  return os;
}
