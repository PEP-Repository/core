// Forward declaration
namespace emscripten { class val; }

namespace pep::weblib {

/// Make `cancel()` a no-op after completion
void StreamSourceWithIndirectCancel(const emscripten::val& streamSource);

}
