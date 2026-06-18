#include <pep/authserver/AuthserverSerializers.hpp>
#include <pep/crypto/CryptoSerializers.hpp>

namespace pep {

TokenRequest Serializer<TokenRequest>::fromProtocolBuffer(proto::TokenRequest&& source) const {
  return TokenRequest{
    std::move(*source.mutable_subject()),
    std::move(*source.mutable_group()),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_expirationtime()))
  };
}

void Serializer<TokenRequest>::moveIntoProtocolBuffer(proto::TokenRequest& dest, TokenRequest value) const {
  *dest.mutable_subject() = std::move(value.subject_);
  *dest.mutable_group() = std::move(value.group_);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_expirationtime(), value.expirationTime_);
}

TokenResponse Serializer<TokenResponse>::fromProtocolBuffer(proto::TokenResponse&& source) const {
  return TokenResponse(std::move(*source.mutable_token()));
}

void Serializer<TokenResponse>::moveIntoProtocolBuffer(proto::TokenResponse& dest, TokenResponse value) const {
  *dest.mutable_token() = std::move(value.token_);
}

}
