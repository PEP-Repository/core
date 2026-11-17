#include <pep/storagefacility/PrefixedStrings.hpp>

#include <stdexcept>
#include <string_view>
#include <utility>

namespace pep {

void detail::PrefixedStringWriter::ensureNoSeparator(std::string_view str, const char* errorMessage) const {
  if (str.contains(separator_)) {
    throw std::runtime_error(errorMessage);
  }
}

void detail::PrefixedStringWriter::output(std::string_view str) const {
  this->ensureNoSeparator(str, "Cannot write string: it contains the separator character");
  for (const auto& prefix : prefixes_) {
    out_ << prefix;
  }
  out_ << str << separator_;
}

void detail::PrefixedStringWriter::operator()(const PrefixedString& prefixedString) const {
  output(prefixedString.string);
}

void detail::PrefixedStringWriter::operator()(const PrefixedStrings& prefixedStrings) const {
  for (const auto& str : prefixedStrings.strings) {
    output(str);
  }
}

void detail::PrefixedStringWriter::operator()(PushPrefix pushPrefix) {
  this->ensureNoSeparator(pushPrefix.prefix, "Cannot push prefix: it contains the separator character");
  prefixes_.push_back(std::move(pushPrefix.prefix));
}

void detail::PrefixedStringWriter::operator()(PopPrefix) {
  if (prefixes_.empty()) {
    throw std::runtime_error("Cannot pop prefix: no prefix has been pushed");
  }
  prefixes_.pop_back();
}

void detail::PrefixedStringWriter::finish() const {
  if (!prefixes_.empty()) {
    throw std::runtime_error("Not all prefixes have been popped");
  }
}

}
