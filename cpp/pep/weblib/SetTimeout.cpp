#include <pep/weblib/SetTimeout.hpp>

#include <pep/utils/Defer.hpp>

#include <emscripten/bind.h>
#include <emscripten/em_js.h>
#include <emscripten/val.h>

#include <cstdint>

using namespace emscripten;
using namespace pep::weblib;

namespace pep::weblib {
struct SetTimeoutContext {
  std::function<void()> callback;
  /// Pointer to self. Keeps instance alive until callback is invoked or timer is canceled.
  std::shared_ptr<SetTimeoutContext> keepMeAlive{};
};
}

namespace {
  
void SetTimeoutInvokeCallback(std::uintptr_t ctxInt) {
  std::move(reinterpret_cast<pep::weblib::SetTimeoutContext*>(ctxInt)->keepMeAlive)->callback();
}

EMSCRIPTEN_BINDINGS(setTimeout) {
  function("pepSetTimeoutInvokeCallback", &SetTimeoutInvokeCallback);
}

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmissing-variable-declarations" // For EM_JS parameters
#endif

EM_JS(EM_VAL, UnaryPlus, (EM_VAL handle), { //language=js
  return Emval.toHandle(+Emval.toValue(handle));
});

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

}

SetTimeoutHandle pep::weblib::SetTimeout(std::chrono::duration<double, std::milli> delay, std::function<void()> callback) {
  auto ctx = std::make_shared<SetTimeoutContext>(SetTimeoutContext{std::move(callback)});
  ctx->keepMeAlive = ctx;

  // We cannot use emscripten_set_timeout, because due to a bug it currently keeps the runtime alive even after emscripten_clear_timeout,
  //  see https://github.com/emscripten-core/emscripten/issues/23763.
  // UnaryPlus is necessary because Node.js returns a `Timeout` object instead of a number, but it can be converted like this.
  auto timerId = val::take_ownership(UnaryPlus(val::global("setTimeout")(
    val::module_property("pepSetTimeoutInvokeCallback"),
    delay.count(),
    val(reinterpret_cast<std::uintptr_t>(ctx.get()))
  ).as_handle())).as<unsigned long>();
  return {timerId, std::move(ctx)};
}

void SetTimeoutHandle::cancel() const {
  if (auto ctx = ctx_.lock()) {
    ctx->keepMeAlive.reset();
    val::global("clearTimeout")(timerId_);
  }
}
