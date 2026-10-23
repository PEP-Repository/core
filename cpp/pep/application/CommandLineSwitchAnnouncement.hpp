#pragma once

#include <functional>
#include <string>

namespace pep {
namespace commandline {

/// \brief The key with which a parameter with explicit name may be specified on the command line (e.g. --loglevel).
class SwitchAnnouncement {
private:
  std::string prefix_;
  std::string text_;

private:
  SwitchAnnouncement(const std::string& prefix, const std::string& text);

public:
  static const std::string StopProcessing;

  explicit SwitchAnnouncement(const std::string& name);
  explicit SwitchAnnouncement(char shorthand);

  constexpr const std::string& getPrefix() const noexcept { return prefix_; }
  constexpr const std::string& getText() const noexcept { return text_; }

  inline std::string string() const { return prefix_ + text_; }
};

inline bool operator ==(const SwitchAnnouncement& lhs, const SwitchAnnouncement& rhs) {
  return lhs.string() == rhs.string();
}

/// \brief Orders announcements by text, ignoring the prefix (so that e.g. "--x" and "-x" are equivalent in a std::set).
/// \remark Deliberately an operator<. Don't define a custom std::less<SwitchAnnouncement> instead:
///         libc++ 22 and later ignore such a custom std::less when searching a std::set, and call operator< directly.
///         See https://github.com/llvm/llvm-project/issues/178847
inline bool operator <(const SwitchAnnouncement& lhs, const SwitchAnnouncement& rhs) {
  return lhs.getText() < rhs.getText();
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

}
