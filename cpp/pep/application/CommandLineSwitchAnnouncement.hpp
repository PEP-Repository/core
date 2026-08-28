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

  [[nodiscard]] auto operator<=>(const SwitchAnnouncement&) const = default;
};

}
}

// Specialize std stuff to enable usage in containers
template <>
struct std::hash<pep::commandline::SwitchAnnouncement> {
  inline size_t operator()(const pep::commandline::SwitchAnnouncement& k) const {
    return std::hash<std::string>()(k.string());
  }
};
