#pragma once

#ifdef _WIN32
# include <pep/utils/Win32Api.hpp>

# include <cstdio>
# include <iostream>

# include <io.h>
# include <fcntl.h>
#endif

namespace pep {

#ifndef _Win32
namespace detail {
struct [[nodiscard, maybe_unused]] SetFileModeNotNecessary {
  SetFileModeNotNecessary() = default;
  SetFileModeNotNecessary(const SetFileModeNotNecessary&) = delete;
  SetFileModeNotNecessary& operator=(const SetFileModeNotNecessary&) = delete;
  SetFileModeNotNecessary(SetFileModeNotNecessary&& other) noexcept = default;
  SetFileModeNotNecessary& operator=(SetFileModeNotNecessary&&) = delete;
};
}
#endif

inline auto SetStdinToBinaryInScope() {
#ifdef _WIN32
  return win32api::SetFileMode(stdin, std::cin.rdbuf(), _O_BINARY);
#else
  return detail::SetFileModeNotNecessary{};
#endif
}

inline auto SetStdoutToBinaryInScope() {
#ifdef _WIN32
  return win32api::SetFileMode(stdout, std::cout.rdbuf(), _O_BINARY);
#else
  return detail::SetFileModeNotNecessary{};
#endif
}

} // namespace pep
