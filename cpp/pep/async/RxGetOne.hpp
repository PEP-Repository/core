#pragma once

#include <pep/async/RxRequireCount.hpp>

namespace pep {

/// Makes sure you get one and only one item back from an RX call.
struct RxGetOne : public RxRequireCount {
  /// \param errorText Custom text to be displayed in the errors when no of multiple items are found.
  RxGetOne(std::string errorText) noexcept : RxRequireCount(1U, 1U, std::move(errorText)) {}
};

}
