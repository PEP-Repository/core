#pragma once

#include <boost/algorithm/string/classification.hpp>

namespace pep {

//NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2781#note_55944
template <typename TRange>
auto IsAnyOf(const TRange& range) {
  return boost::algorithm::is_any_of(range);
}
//NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks) See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2781#note_55944

}
