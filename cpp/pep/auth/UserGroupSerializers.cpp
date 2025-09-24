#include <pep/auth/UserGroupSerializers.hpp>

namespace pep {

UserGroup Serializer<UserGroup>::fromProtocolBuffer(proto::UserGroup&& source) const {
  UserGroup result;
  result.mName = std::move(*source.mutable_name());
  if (source.has_max_auth_validity_seconds()) {
    result.mMaxAuthValidity = std::chrono::seconds(source.max_auth_validity_seconds());
  }
  return result;
}

void Serializer<UserGroup>::moveIntoProtocolBuffer(proto::UserGroup& dest, UserGroup value) const {
  *dest.mutable_name() = std::move(value.mName);
  if (value.mMaxAuthValidity) {
    dest.set_max_auth_validity_seconds(static_cast<uint64_t>(value.mMaxAuthValidity->count()));
  }
}

}