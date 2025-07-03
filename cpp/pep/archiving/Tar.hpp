#pragma once

#include <filesystem>

#include <pep/utils/Shared.hpp>
#include <pep/archiving/Archive.hpp>

struct archive;

namespace pep {

class Tar : public SharedConstructor<Tar>, public Archive {
  friend class SharedConstructor<Tar>;
public:
  Tar(const Tar&) = delete; //prevent copies of the archive* pointer
  ~Tar() override;
  void nextEntry(const std::filesystem::path& path, int64_t size) override;
  void writeData(const char* c, const std::streamsize l) override;
  void writeData(std::string_view data) override;
  void closeEntry() override {}
  bool expectsSizeUpFront() override { return true; }

  static void Extract(std::istream& stream, const std::filesystem::path& outputDirectory);

private:
  Tar(std::shared_ptr<std::ostream> stream);

  std::shared_ptr<std::ostream> mStream;
  archive* mArchive;
};

}
