#include <pep/weblib/Emscripten_fwd.hpp>

namespace pep::weblib {

/// Make `cancel()` a no-op after completion
void StreamSourceWithIndirectCancel(const emscripten::val& streamSource);

}
