#include <pep/archiving/HashedArchive.hpp>
#include <pep/archiving/NOPArchive.hpp>

#include <boost/iostreams/device/mapped_file.hpp>

#include <filesystem>

namespace pep {

HashedArchive::HashedArchive(std::shared_ptr<Archive> archive) : mArchive(archive), mHasher(std::make_unique<XxHasher>(DOWNLOAD_HASH_SEED)) {}

void HashedArchive::nextEntry(const std::filesystem::path& path, int64_t size) {
  if(!mCurrentEntry.empty()) {
    throw std::runtime_error("Called nextEntry() when previous entry has not yet been closed");
  }
  mCurrentEntry = path.string();
  mArchive->nextEntry(path, size);
}
void HashedArchive::writeData(const char* c, const std::streamsize l) {
  if(mCurrentEntry.empty()) {
    throw std::runtime_error("HashedArchive: cannot write data when nextEntry has not been called yet.");
  }
  mHasher->update(c, static_cast<size_t>(l));
  mArchive->writeData(c, l);
 }

void HashedArchive::writeData(std::string_view data) {
  writeData(data.data(), static_cast<std::streamsize>(data.length()));
}

void HashedArchive::closeEntry() {
  if (mCurrentEntry.empty()) {
    throw std::runtime_error("Cannot close entry when no entry has yet been opened with nextEntry()");
  }
  auto [it, inserted] = mHashes.emplace(mCurrentEntry, mHasher->digest());
  if (!inserted) {
    throw std::runtime_error("HashedArchive: Multiple entries with the same name: " + mCurrentEntry);
  }
  mCurrentEntry = "";
  mHasher = std::make_unique<XxHasher>(DOWNLOAD_HASH_SEED);
  mArchive->closeEntry();
}

bool HashedArchive::expectsSizeUpFront() {
  return mArchive->expectsSizeUpFront();
}

XxHasher::Hash HashedArchive::digest() const {
  XxHasher hasher(DOWNLOAD_HASH_SEED);
  for(auto& [key, value] : mHashes) {
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
          newSubpath,
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
  HashedArchive hashedArchive(std::make_shared<NOPArchive>());
  hashedArchive.processDirectory(path, std::filesystem::path());
  return hashedArchive.digest();
}

}
