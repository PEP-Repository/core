#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pep {

std::string PackUint8(uint8_t x);
std::string PackUint32BE(uint32_t x);
std::string PackUint64BE(uint64_t x);
uint32_t UnpackUint32BE(std::string_view x);
uint64_t UnpackUint64BE(std::string_view x);

// TODO: remove: test finding (modernize-use-bool-literals) for the clang-tidy Code Quality report. This header is
// included by many source files, but the report should list the finding once.
inline bool ClangTidyCodeQualityTest() { return 1; }

}
