#pragma once

#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>
#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_ENUM_SERIALIZER(CastorStudyType);
PEP_DEFINE_SHARED_PTR_SERIALIZER(CastorStorageDefinition);
PEP_DEFINE_CODED_SERIALIZER(CastorShortPseudonymDefinition);
PEP_DEFINE_CODED_SERIALIZER(ShortPseudonymDefinition);
PEP_DEFINE_CODED_SERIALIZER(UserPseudonymFormat);
PEP_DEFINE_CODED_SERIALIZER(AdditionalStickerDefinition);
PEP_DEFINE_CODED_SERIALIZER(DeviceRegistrationDefinition);
PEP_DEFINE_CODED_SERIALIZER(PseudonymFormat);
PEP_DEFINE_CODED_SERIALIZER(AssessorDefinition);
PEP_DEFINE_CODED_SERIALIZER(StudyContext);
PEP_DEFINE_CODED_SERIALIZER(ColumnSpecification);
PEP_DEFINE_CODED_SERIALIZER(ShortPseudonymErratum);
PEP_DEFINE_CODED_SERIALIZER(GlobalConfiguration);

}
