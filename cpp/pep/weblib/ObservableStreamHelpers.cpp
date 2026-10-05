#include <pep/weblib/ObservableStreamHelpers.hpp>

#include <emscripten/em_js.h>
#include <emscripten/val.h>

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmissing-variable-declarations" // For EM_JS parameters
#endif

EM_JS(void, PepStreamSourceSetIndirectCancel, (emscripten::EM_VAL streamSourceHandle), { //language=js
  const streamSource = Emval.toValue(streamSourceHandle);
  const originalCancel = streamSource.cancel;
  streamSource.cancel = reason => {
    // Only call if still present
    if (streamSource.cancel) {
      return originalCancel.call(streamSource, reason);
    } else {
      console.debug('Skipping cancel for completed stream with reason', reason);
    }
  };
});

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

void pep::weblib::StreamSourceWithIndirectCancel(const emscripten::val& streamSource) {
  PepStreamSourceSetIndirectCancel(streamSource.as_handle());
}
