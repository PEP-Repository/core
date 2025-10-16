#pragma once

#include <pep/versioning/Version.hpp>
#include <pep/crypto/Signed.hpp>
#include <pep/crypto/Timestamp.hpp>

namespace pep {

class VersionRequest {
};

class VersionResponse {
public:
  BinaryVersion binary;
  std::optional<ConfigVersion> config;
};

class PingRequest {
public:
  explicit PingRequest(uint64_t id) : mId(id) { }
  uint64_t mId;
};

class PingResponse {
public:
  explicit PingResponse(uint64_t id) : mId(id), mTimestamp(Timestamp::now()) { }

  uint64_t mId;
  Timestamp mTimestamp;
};

using SignedPingResponse = Signed<PingResponse>;

}
