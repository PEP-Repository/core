#include <pep/application/CommandLineSwitchAnnouncement.hpp>

#include <algorithm>
#include <cassert>

namespace pep {
namespace commandline {

namespace {

std::string CharToString(char c) {
  return std::string{c};
}

const char SwitchPrefixCharacter = '-';
const std::string SwitchShorthandPrefix = CharToString(SwitchPrefixCharacter);
const std::string SwitchNamePrefix = SwitchShorthandPrefix + SwitchShorthandPrefix;

}

const std::string SwitchAnnouncement::StopProcessing = SwitchNamePrefix;

SwitchAnnouncement::SwitchAnnouncement(const std::string& prefix, const std::string& text)
  : prefix_(prefix), text_(text) {
  assert(!text_.empty());
  assert(text_[0] != SwitchPrefixCharacter);
  assert(!std::any_of(text_.begin(), text_.end(), [](char c) {return !std::isprint(c); }));
}

SwitchAnnouncement::SwitchAnnouncement(const std::string& name)
  : SwitchAnnouncement(SwitchNamePrefix, name) {
  assert(name.size() > 1U);
}

SwitchAnnouncement::SwitchAnnouncement(char shorthand)
  : SwitchAnnouncement(SwitchShorthandPrefix, CharToString(shorthand)) {
}

}
}
