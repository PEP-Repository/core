#include <pep/application/CommandLineSwitchAnnouncement.hpp>

#include <algorithm>
#include <cassert>

namespace pep {
namespace commandline {

namespace {

std::string CharToString(char c) {
  return std::string{c};
}

const char SWITCH_PREFIX_CHARACTER = '-';
const std::string SWITCH_SHORTHAND_PREFIX = CharToString(SWITCH_PREFIX_CHARACTER);
const std::string SWITCH_NAME_PREFIX = SWITCH_SHORTHAND_PREFIX + SWITCH_SHORTHAND_PREFIX;

}

const std::string SwitchAnnouncement::STOP_PROCESSING = SWITCH_NAME_PREFIX;

SwitchAnnouncement::SwitchAnnouncement(const std::string& prefix, const std::string& text)
  : mPrefix(prefix), mText(text) {
  assert(!mText.empty());
  assert(mText[0] != SWITCH_PREFIX_CHARACTER);
  assert(!std::any_of(mText.begin(), mText.end(), [](char c) {return !std::isprint(c); }));
}

SwitchAnnouncement::SwitchAnnouncement(const std::string& name)
  : SwitchAnnouncement(SWITCH_NAME_PREFIX, name) {
  assert(name.size() > 1U);
}

SwitchAnnouncement::SwitchAnnouncement(char shorthand)
  : SwitchAnnouncement(SWITCH_SHORTHAND_PREFIX, CharToString(shorthand)) {
}

}
}
