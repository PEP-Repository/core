#pragma once

#include <functional>
#include <string>

namespace pep {
namespace commandline {

/*!
 * \brief The key with which a parameter with explicit name may be specified on the command line (e.g. --loglevel).
 */
class SwitchAnnouncement {
private:
  std::string mPrefix;
  std::string mText;

private:
  SwitchAnnouncement(const std::string& prefix, const std::string& text);

public:
  static const std::string STOP_PROCESSING;

  explicit SwitchAnnouncement(const std::string& name);
  explicit SwitchAnnouncement(char shorthand);

  constexpr const std::string& getPrefix() const noexcept { return mPrefix; }
  constexpr const std::string& getText() const noexcept { return mText; }

  inline std::string string() const { return mPrefix + mText; }
};

inline bool operator ==(const SwitchAnnouncement& lhs, const SwitchAnnouncement& rhs) {
  return lhs.string() == rhs.string();
}

}
}

// Specialize std stuff to enable usage in containers
namespace std {

template <>
struct hash<pep::commandline::SwitchAnnouncement> {
  inline size_t operator()(const pep::commandline::SwitchAnnouncement& k) const {
    return std::hash<std::string>()(k.string());
  }
};

template <>
struct less<pep::commandline::SwitchAnnouncement> {
  inline constexpr bool operator()(const pep::commandline::SwitchAnnouncement& lhs, const pep::commandline::SwitchAnnouncement& rhs) const {
    return std::less<std::string>()(lhs.getText(), rhs.getText());
  }
};

}
