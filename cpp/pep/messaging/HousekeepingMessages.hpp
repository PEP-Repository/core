#pragma once

#include <pep/versioning/Version.hpp>
#include <pep/auth/Signed.hpp>
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
  PingRequest(); // sets ID to a random value
  explicit PingRequest(uint64_t id) : mId(id) {}
  uint64_t mId;
};

class PingResponse {
public:
  explicit PingResponse(uint64_t id) : mId(id), mTimestamp(TimeNow()) { }

  uint64_t mId;
  Timestamp mTimestamp;

  void validate(const PingRequest& isReplyTo) const;
};

using SignedPingResponse = Signed<PingResponse>;

}
