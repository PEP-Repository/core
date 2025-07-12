#pragma once

// rxcpp::observable<void> doesn't work as expected; so instead we use
// a dummy class FakeVoid instead of void.  See #713

namespace pep {

class FakeVoid { };

}
