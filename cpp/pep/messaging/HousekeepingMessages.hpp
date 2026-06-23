#pragma once

#include <pep/versioning/Version.hpp>
#include <pep/auth/Signed.hpp>
#include <pep/crypto/Timestamp.hpp>

namespace pep {

struct VersionRequest {};

struct VersionResponse {
  BinaryVersion binary;
  std::optional<ConfigVersion> config;
};

class PingRequest {
public:
  PingRequest(); // sets ID to a random value
  explicit PingRequest(uint64_t id) : id_(id) {}

  uint64_t id() const noexcept { return id_; }

private:
  uint64_t id_;
};

struct PingResponse {
  uint64_t id;
  Timestamp timestamp;

  void validate(const PingRequest& isReplyTo) const;
};

using SignedPingResponse = Signed<PingResponse>;

}
