#include <pep/server/CertificateRenewalSerializers.hpp>

namespace pep {

CsrResponse Serializer<CsrResponse>::fromProtocolBuffer(proto::CsrResponse&& source) const {
  return CsrResponse(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_csr())));
}

void Serializer<CsrResponse>::moveIntoProtocolBuffer(proto::CsrResponse& dest, CsrResponse value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_csr(), value.getCsr());
}

CertificateReplacementRequest Serializer<CertificateReplacementRequest>::fromProtocolBuffer(proto::CertificateReplacementRequest&& source) const {
  return CertificateReplacementRequest(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_certificate_chain())),
    source.force());
}

void Serializer<CertificateReplacementRequest>::moveIntoProtocolBuffer(proto::CertificateReplacementRequest& dest, CertificateReplacementRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_certificate_chain(), value.getCertificateChain());
  dest.set_force(value.force());
}

CertificateReplacementCommitRequest Serializer<CertificateReplacementCommitRequest>::fromProtocolBuffer(proto::CertificateReplacementCommitRequest&& source) const {
  return CertificateReplacementCommitRequest(
    Serialization::FromProtocolBuffer(std::move(*source.mutable_certificate_chain())));
}

void Serializer<CertificateReplacementCommitRequest>::moveIntoProtocolBuffer(proto::CertificateReplacementCommitRequest& dest, CertificateReplacementCommitRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_certificate_chain(), value.getCertificateChain());
}

}