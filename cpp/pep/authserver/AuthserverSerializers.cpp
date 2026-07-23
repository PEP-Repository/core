#include <pep/authserver/AuthserverSerializers.hpp>
#include <pep/serialization/TimestampSerializer.hpp>

namespace pep {

TokenRequest Serializer<TokenRequest>::fromProtocolBuffer(proto::TokenRequest&& source) const {
  return TokenRequest{
    .subject = std::move(*source.mutable_subject()),
    .group = std::move(*source.mutable_group()),
    .expirationTime = Serialization::FromProtocolBuffer(std::move(*source.mutable_expirationtime()))
  };
}

void Serializer<TokenRequest>::moveIntoProtocolBuffer(proto::TokenRequest& dest, TokenRequest value) const {
  *dest.mutable_subject() = std::move(value.subject);
  *dest.mutable_group() = std::move(value.group);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_expirationtime(), value.expirationTime);
}

TokenResponse Serializer<TokenResponse>::fromProtocolBuffer(proto::TokenResponse&& source) const {
  return TokenResponse{
    .token = std::move(*source.mutable_token())
  };
}

void Serializer<TokenResponse>::moveIntoProtocolBuffer(proto::TokenResponse& dest, TokenResponse value) const {
  *dest.mutable_token() = std::move(value.token);
}

}
