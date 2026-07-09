#include <pep/archiving/HashedArchive.hpp>
#include <pep/archiving/NoOpArchive.hpp>

#include <boost/iostreams/device/mapped_file.hpp>

#include <filesystem>

namespace pep {

HashedArchive::HashedArchive(std::shared_ptr<Archive> archive) : archive_(archive), hasher_(std::make_unique<XxHasher>(DownloadHashSeed)) {}

void HashedArchive::nextEntry(const SafePath& path, int64_t size) {
  if(!currentEntry_.empty()) {
    throw std::runtime_error("Called nextEntry() when previous entry has not yet been closed");
  }
  currentEntry_ = path.path().string();
  archive_->nextEntry(path, size);
}
void HashedArchive::writeData(const char* c, const std::streamsize l) {
  if(currentEntry_.empty()) {
    throw std::runtime_error("HashedArchive: cannot write data when nextEntry has not been called yet.");
  }
  hasher_->update(c, static_cast<size_t>(l));
  archive_->writeData(c, l);
 }

void HashedArchive::writeData(std::string_view data) {
  writeData(data.data(), static_cast<std::streamsize>(data.length()));
}

void HashedArchive::closeEntry() {
  if (currentEntry_.empty()) {
    throw std::runtime_error("Cannot close entry when no entry has yet been opened with nextEntry()");
  }
  auto [it, inserted] = hashes_.emplace(currentEntry_, hasher_->digest());
  if (!inserted) {
    throw std::runtime_error("HashedArchive: Multiple entries with the same name: " + currentEntry_);
  }
  currentEntry_ = "";
  hasher_ = std::make_unique<XxHasher>(DownloadHashSeed);
  archive_->closeEntry();
}

bool HashedArchive::expectsSizeUpFront() {
  return archive_->expectsSizeUpFront();
}

XxHasher::Hash HashedArchive::digest() const {
  XxHasher hasher(DownloadHashSeed);
  for(auto& [key, value] : hashes_) {
    hasher.update(key.data(), key.length());
    hasher.update(&value, sizeof(XxHasher::Hash));
  }
  return hasher.digest();
}

void HashedArchive::processDirectory(const std::filesystem::path& path, const::std::filesystem::path& subpath) {
  for (auto &entry : std::filesystem::directory_iterator(path)) {
    const std::filesystem::path& inpath = entry.path();
    std::filesystem::path newSubpath =
        subpath / inpath.filename();
    if (!std::filesystem::is_directory(entry)) {
      nextEntry(
          SafePath::FromTrusted(newSubpath),
          static_cast<int64_t>(std::filesystem::file_size(inpath)));
      if (std::filesystem::file_size(inpath) > 0){
        //mmap file content and turn it into a string_view
        boost::iostreams::mapped_file content(inpath.string());
        std::string_view stringifiedContent{content.data(), content.size()};
        writeData(stringifiedContent);
      }

      closeEntry();
    }
    else {
      processDirectory(inpath, newSubpath);
    }
  }
}


XxHasher::Hash HashedArchive::HashDirectory(const std::filesystem::path& path) {
  HashedArchive hashedArchive(std::make_shared<NoOpArchive>());
  hashedArchive.processDirectory(path, std::filesystem::path());
  return hashedArchive.digest();
}

}
