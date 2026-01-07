#include <pep/utils/File.hpp>
#include <pep/utils/Log.hpp>

#include <array>
#include <string>
#include <fstream>
#include <regex>
#include <cassert>
#include <iostream>

namespace pep {

namespace {

const std::streamsize DISK_IO_BUFFERSIZE{ 4096 };

}

std::string ReadFile(const char* path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("File " + std::filesystem::absolute(path).string() + " does not exist");
  }

  std::ifstream in(path, std::ios::in | std::ios::binary | std::ios::ate); // Binary mode prevents Windows from stopping when it encounters 0x1A (Ctrl+Z). See #1055
  in.exceptions(in.exceptions() | std::ios::failbit);
  auto size = in.tellg();
  assert(size >= 0);
  in.seekg(0);
  std::string content(static_cast<std::string::size_type>(size), '\0');
  in.read(content.data(), static_cast<std::streamsize>(content.size())); // will throw on failures since we added failbit to the exception mask
  in.close();
  return content;
}

std::string ReadFile(const std::string& path) {
  return ReadFile(path.c_str());
}

std::string ReadFile(const std::filesystem::path& path) {
  return ReadFile(path.string());
}

std::optional<std::string> ReadFileIfExists(const std::filesystem::path& path) {
  if (std::filesystem::exists(path)) {
    return ReadFile(path.string());
  }
  return std::nullopt;
}

void WriteFile(const std::filesystem::path& path, const std::string& content) {
  try {
    std::clog << "start" << std::endl;
    std::ofstream output(path.string(), std::ios::out | std::ios::binary);
    std::clog << "ofstream constructed" << std::endl;
    output.exceptions(output.exceptions() | std::ios::failbit);
    std::clog << "exception mask set" << std::endl;
    output << content;
    std::clog << "content written" << std::endl;
    output.close();
    std::clog << "closed" << std::endl;
  }
  catch (std::ios_base::failure& e) {
    LOG("WriteFile", error) << "IO Failure: " << e.what() << ". Error code " << e.code() << ": " << e.code().message();
    throw;
  }
  catch (std::exception& e) {
    LOG("WriteFile", error) << "Exception: " << e.what();
    throw;
  }
}

bool IsValidFileExtension(const std::string& extension) {
  const std::regex extensionRegex("(\\.[A-Za-z0-9]+)+");
  return std::regex_match(extension, extensionRegex);
}

void IstreamToDestination(std::istream& in, std::function<void(const char * c, const std::streamsize len)> writeToDestination) {
  while (!in.eof()) {
    if (in.fail()) {
      throw std::runtime_error("Reading from stream failed"); // Exception meant to be rethrown with more information.
    }
    std::array<char, DISK_IO_BUFFERSIZE> buffer{};
    in.read(buffer.data(), buffer.size());
    if (in.gcount() > 0) {
      writeToDestination(buffer.data(), in.gcount());
    }
    else {
      break;
    }
  }
}
}
