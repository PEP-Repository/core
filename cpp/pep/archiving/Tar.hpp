#pragma once

#include <pep/archiving/Archive.hpp>
#include <pep/utils/CheckedPath.hpp>
#include <pep/utils/Shared.hpp>

#include <cstdint>
#include <filesystem>
#include <ios>
#include <istream>
#include <memory>
#include <ostream>
#include <string_view>

struct archive; // Forward declares type provided by libarchive

namespace pep {

class Tar : public SharedConstructor<Tar>, public Archive {
  friend class SharedConstructor<Tar>;
public:
  Tar(const Tar&) = delete; //prevent copies of the archive* pointer
  ~Tar() override;
  void nextEntry(const CheckedPath& path, int64_t size) override;
  void writeData(const char* c, const std::streamsize l) override;
  void writeData(std::string_view data) override;
  void closeEntry() override {}
  bool expectsSizeUpFront() override { return true; }

  static void Extract(std::istream& stream, const std::filesystem::path& outputDirectory);

private:
  Tar(std::shared_ptr<std::ostream> stream);

  std::shared_ptr<std::ostream> stream_;
  archive* archive_;
};

}
