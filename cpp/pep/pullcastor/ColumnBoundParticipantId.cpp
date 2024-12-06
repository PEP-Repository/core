#include <pep/pullcastor/ColumnBoundParticipantId.hpp>

#include <boost/functional/hash.hpp>

namespace pep {
namespace castor {

ColumnBoundParticipantId::ColumnBoundParticipantId(const std::string& columnName, const std::string& participantId)
  : mColumnName(columnName), mParticipantId(participantId) {
}

bool operator==(const ColumnBoundParticipantId& lhs, const ColumnBoundParticipantId& rhs) {
  return (lhs.getColumnName() == rhs.getColumnName())
    && (lhs.getParticipantId() == rhs.getParticipantId());
}

}
}

namespace std {

size_t hash<pep::castor::ColumnBoundParticipantId>::operator()(const pep::castor::ColumnBoundParticipantId& cbpId) const {
  size_t result = 0U;

  // Use of boost::hash_combine suggested by https://en.cppreference.com/w/cpp/utility/hash
  boost::hash_combine(result, cbpId.getColumnName());
  boost::hash_combine(result, cbpId.getParticipantId());

  return result;
}

}
