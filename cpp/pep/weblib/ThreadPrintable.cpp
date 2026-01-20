#include <pep/weblib/ThreadPrintable.hpp>

#include <emscripten/threading.h>
#include <emscripten/val.h>

std::ostream& pep::weblib::operator<<(std::ostream& os, const CurrentThreadPrintable&) {
  const auto flags = os.flags();
  os << std::hex << "0x" << ::pthread_self();
  if (::emscripten_is_main_runtime_thread()) {
    os << " main";
  } else {
    emscripten::val name = emscripten::val::global("name");
    if (name.isString()) {
      os << ' ' << name.as<std::string>();
    }
  }
  os.flags(flags);
  return os;
}

std::ostream& pep::weblib::operator<<(std::ostream& os, const ThreadPrintable& self) {
  const auto flags = os.flags();
  os << std::hex << "0x" << self.thread_;
  if (self.thread_ == ::emscripten_main_runtime_thread_id()) {
    os << " (main)";
  }
  os.flags(flags);
  return os;
}
