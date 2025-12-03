#include <pep/server/CertificateRenewalSerializers.hpp>

namespace pep {

CsrResponse Serializer<CsrResponse>::fromProtocolBuffer(proto::CsrResponse&& source) const {
  CsrResponse result;
  result.mCsr = Serialization::FromProtocolBuffer(std::move(*source.mutable_csr()));
  return result;
}

void Serializer<CsrResponse>::moveIntoProtocolBuffer(proto::CsrResponse& dest, CsrResponse value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_csr(), std::move(value.mCsr));
}

CertificateReplacementRequest Serializer<CertificateReplacementRequest>::fromProtocolBuffer(proto::CertificateReplacementRequest&& source) const {
  CertificateReplacementRequest result;
  result.mCertificateChain = Serialization::FromProtocolBuffer(std::move(*source.mutable_certificate_chain()));
  result.mForce = source.force();
  return result;
}

void Serializer<CertificateReplacementRequest>::moveIntoProtocolBuffer(proto::CertificateReplacementRequest& dest, CertificateReplacementRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_certificate_chain(), std::move(value.mCertificateChain));
  dest.set_force(value.mForce);
}

CertificateReplacementCommitRequest Serializer<CertificateReplacementCommitRequest>::fromProtocolBuffer(proto::CertificateReplacementCommitRequest&& source) const {
  CertificateReplacementCommitRequest result;
  result.mCertificateChain = Serialization::FromProtocolBuffer(std::move(*source.mutable_certificate_chain()));
  return result;
}

void Serializer<CertificateReplacementCommitRequest>::moveIntoProtocolBuffer(proto::CertificateReplacementCommitRequest& dest, CertificateReplacementCommitRequest value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_certificate_chain(), std::move(value.mCertificateChain));
}

}