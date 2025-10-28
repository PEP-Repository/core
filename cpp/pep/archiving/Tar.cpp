#include <pep/archiving/Tar.hpp>

#include <pep/utils/Log.hpp>

#include <archive.h>
#include <archive_entry.h>

#include <fstream>
#include <sstream>

using namespace pep;

namespace {

const std::string LOG_TAG ("Tar");

const size_t RETRIES = 3;

struct TarCtx {
   std::istream& stream;
   std::array<char, 10240> buf{};

   TarCtx(std::istream &stream) : stream(stream) {}
};

int open_callback(archive* archive, void* clientData) {
 return ARCHIVE_OK;
}

la_ssize_t write_callback(archive* archive, void* clientData, const void* buffer, size_t length) {
  auto stream = static_cast<std::ostream*>(clientData);
  stream->write(static_cast<const char*>(buffer), static_cast<std::streamsize>(length));
  if(stream->fail()) {
    archive_set_error(archive, EIO, "Error writing archive to stream");
    return -1;
  }
  return static_cast<la_ssize_t>(length);
}

la_ssize_t read_callback(archive* archive, void* clientData, const void** buffer) {
  auto ctx  = static_cast<TarCtx*>(clientData);
  if(ctx->stream.eof()) {
    return 0;
  }
  ctx->stream.read(ctx->buf.data(), static_cast<std::streamsize>(ctx->buf.size()));
  if(ctx->stream.fail()) {
    archive_set_error(archive, EIO, "Error reading archive from stream");
    return -1;
  }
  *buffer = &ctx->buf;
  return static_cast<la_ssize_t>(ctx->stream.gcount());
}

int close_callback(archive* archive, void* clientData) {
  return ARCHIVE_OK;
}

archive_entry* readNextHeader(archive* archive) {
  archive_entry* entry{};
  int result = archive_read_next_header(archive, &entry);
  for(unsigned int retry = 1; result == ARCHIVE_RETRY && retry <= RETRIES; ++retry) {
    LOG(LOG_TAG, warning) << "Retry " << retry << " of " << RETRIES << " after warning while reading tar entry header: " << archive_errno(archive) << " - " << archive_error_string(archive);
    result = archive_read_next_header(archive, &entry);
  }
  std::ostringstream oss;
  switch(result) {
    case ARCHIVE_RETRY:
      oss << "Error while reading tar entry header. Too many retries for error: " << archive_errno(archive) << " - " << archive_error_string(archive);
      throw std::runtime_error(oss.str());
    case ARCHIVE_WARN:
      LOG(LOG_TAG, warning) << "Warning while reading tar entry header: " << archive_errno(archive) << " - " << archive_error_string(archive);
      return entry;
    case ARCHIVE_FATAL:
      oss << "Error while reading tar entry header: " << archive_errno(archive) << " - " << archive_error_string(archive);
      throw std::runtime_error(oss.str());
    case ARCHIVE_EOF:
      return nullptr;
    case ARCHIVE_OK:
      return entry;
    default:
      oss << "Error while reading tar entry header: unexpecter return code: " << result;
      throw std::runtime_error(oss.str());
  }
}

bool readBlockToStream(archive* archive, std::ostream& out) {
  const void *buff{};
  size_t len{};
  la_int64_t offset{};

  int result = archive_read_data_block(archive, &buff, &len, &offset);
  for(unsigned int retry = 1; result == ARCHIVE_RETRY && retry <= RETRIES; ++retry) {
    LOG(LOG_TAG, warning) << "Retry " << retry << " of " << RETRIES << " after warning while reading tar entry header: " << archive_errno(archive) << " - " << archive_error_string(archive);
    result = archive_read_data_block(archive, &buff, &len, &offset);
  }
  std::ostringstream oss;
  switch(result) {
    case ARCHIVE_RETRY:
      oss << "Error while reading tar entry header. Too many retries for error: " << archive_errno(archive) << " - " << archive_error_string(archive);
      throw std::runtime_error(oss.str());
    case ARCHIVE_WARN:
      LOG(LOG_TAG, warning) << "Warning while reading tar entry header: " << archive_errno(archive) << " - " << archive_error_string(archive);
      break;
    case ARCHIVE_FATAL:
      oss << "Error while reading tar entry header: " << archive_errno(archive) << " - " << archive_error_string(archive);
      throw std::runtime_error(oss.str());
    case ARCHIVE_EOF:
      return false;
    case ARCHIVE_OK:
      break;
    default:
      oss << "Error while reading tar entry header: unexpecter return code: " << result;
      throw std::runtime_error(oss.str());
  }

  while(out.tellp() != offset) {
    out.put(0);
  }
  out.write(static_cast<const char*>(buff), static_cast<std::streamsize>(len));
  return true;
}

} // end namespace


Tar::Tar(std::shared_ptr<std::ostream> stream) : mStream(stream), mArchive(archive_write_new()) {
  if(archive_write_set_format_pax_restricted(mArchive) != ARCHIVE_OK) {
    std::ostringstream oss;
    oss << "Error while setting tar format to pax_restricted: " << archive_errno(mArchive) << " - " << archive_error_string(mArchive);
    throw std::runtime_error(oss.str());
  }

  if(archive_write_open(mArchive, mStream.get(), open_callback, write_callback, close_callback) != ARCHIVE_OK) {
    std::ostringstream oss;
    oss << "Error opening tar file for writing: " << archive_errno(mArchive) << " - " << archive_error_string(mArchive);
    throw std::runtime_error(oss.str());
  }
}

Tar::~Tar() {
  archive_write_close(mArchive);
  archive_write_free(mArchive);
}


void Tar::nextEntry(const std::filesystem::path& path, int64_t size) {
  const std::string& pathString = path.generic_string();

  archive_entry* entry = archive_entry_new();
  archive_entry_set_pathname(entry, pathString.c_str());
  archive_entry_set_size(entry, size);
  archive_entry_set_filetype(entry, AE_IFREG);
  archive_entry_set_perm(entry, 0644);
  int result = archive_write_header(mArchive, entry);
  for(unsigned int retry = 1; result == ARCHIVE_RETRY && retry <= RETRIES; ++retry) {
    LOG(LOG_TAG, warning) << "Retry " << retry << " of " << RETRIES << " after warning while writing tar entry header: " << archive_errno(mArchive) << " - " << archive_error_string(mArchive);
    result = archive_write_header(mArchive, entry);
  }
  if(result == ARCHIVE_WARN) {
      LOG(LOG_TAG, warning) << "Warning while writing tar entry header: " << archive_errno(mArchive) << " - " << archive_error_string(mArchive);
  }
  else if(result == ARCHIVE_RETRY) {
      std::ostringstream oss;
      oss << "Error while writing tar entry header. Too many retries for error: " << archive_errno(mArchive) << " - " << archive_error_string(mArchive);
      throw std::runtime_error(oss.str());
  }
  else if(result == ARCHIVE_FATAL) {
      std::ostringstream oss;
      oss << "Error while writing tar entry header: " << archive_errno(mArchive) << " - " << archive_error_string(mArchive);
      throw std::runtime_error(oss.str());
  }
  archive_entry_free(entry);
}



void Tar::writeData(std::string_view data) {

  if(archive_write_data(mArchive, data.data(), data.length()) < 0) {
    std::ostringstream oss;
    oss << "Error writing data to tar: " << archive_errno(mArchive) << " - " << archive_error_string(mArchive);
    throw std::runtime_error(oss.str());
  }
}

void Tar::writeData(const char* c, const std::streamsize l) {
  assert(l >= 0);
  return this->writeData(std::string_view(c, static_cast<size_t>(l)));
}

void Tar::Extract(std::istream& stream, const std::filesystem::path& outputDirectory) {
  TarCtx ctx(stream);

  archive* archive = archive_read_new();
  archive_read_support_format_tar(archive);
  if(archive_read_open(archive, &ctx, open_callback, read_callback, close_callback) != ARCHIVE_OK) {
    std::ostringstream oss;
    oss << "Error opening tar stream for reading: " << archive_errno(archive) << " - " << archive_error_string(archive);
    throw std::runtime_error(oss.str());
  }
  archive_entry* archive_entry{};
  while( (archive_entry = readNextHeader(archive)) ) {
    std::filesystem::path outpath = outputDirectory / archive_entry_pathname(archive_entry);
    std::filesystem::create_directories(outpath.parent_path());
    std::ofstream out(
      outpath.string(),
      std::ios::binary);
    if(!out) {
      std::ostringstream oss;
      oss << "Error opening output file " << outpath << " for extracting";
      throw std::runtime_error(oss.str());
    }
    while(readBlockToStream(archive, out)) {}
  }
  if(archive_read_free(archive) != ARCHIVE_OK) {
    std::ostringstream oss;
    oss << "Error freeing tar stream after reading: " << archive_errno(archive) << " - " << archive_error_string(archive);
    throw std::runtime_error(oss.str());
  }
}
