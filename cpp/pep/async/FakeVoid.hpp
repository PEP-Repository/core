#pragma once

// rxcpp::observable<void> doesn't work as expected; so instead we use
// a dummy struct FakeVoid instead of void.  See #713

namespace pep {

struct FakeVoid { };

}
