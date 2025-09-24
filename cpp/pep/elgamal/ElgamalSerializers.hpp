#pragma once

#include <pep/elgamal/CurvePoint.hpp>
#include <pep/elgamal/CurveScalar.hpp>
#include <pep/elgamal/ElgamalEncryption.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>

#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(CurvePoint);
PEP_DEFINE_CODED_SERIALIZER(CurveScalar);
PEP_DEFINE_CODED_SERIALIZER(ElgamalEncryption);

}
