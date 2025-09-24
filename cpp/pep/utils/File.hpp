#pragma once

#include <functional>
#include <optional>
#include <filesystem>


namespace pep {

std::string ReadFile(const char* path);
std::string ReadFile(const std::string& path);
std::string ReadFile(const std::filesystem::path& path);
std::optional<std::string> ReadFileIfExists(const std::filesystem::path& path);
void WriteFile(const std::filesystem::path& path, const std::string& content);
bool IsValidFileExtension(const std::string& extension);

/* \brief Reads a std::istream, iteratively filling a buffer. The writeToDestination lambda function can take this buffer (as char* and its length) and 
 * define what should be done with it.
 */
void IstreamToDestination(std::istream& in, std::function<void(const char* c, const std::streamsize len)> writeToDestination);
}


