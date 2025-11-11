/** @var {Object} Module */
/** @var {boolean} ENVIRONMENT_IS_WORKER */

// Retrieve exception message on background threads, even in Release mode.
// See https://github.com/emscripten-core/emscripten/issues/18016
if (ENVIRONMENT_IS_WORKER) {
  /** Are we using Emscripten EH instead of WASM EH? */
  const EmscriptenExceptionHandling = false;

  /**
   * Transforms a WASM exception into an Error with the correct message, and stack when available.
   * After obtaining message, frees the WASM exception object.
   * @param {Object} mod
   * @param {WebAssembly.Exception} wasmEx
   * @returns {Error}
   */
  function handleWasmExceptionForModule(mod, wasmEx) {
    if (EmscriptenExceptionHandling) {
      //XXX When using Emscripten EH, we should call incrementExceptionRefcount first,
      // see https://github.com/emscripten-core/emscripten/issues/17115
      mod.incrementExceptionRefcount(wasmEx);
    }
    try {
      const [type, message] = mod.getExceptionMessage(wasmEx);
      const error = new Error(message || type, {cause: wasmEx});
      if (wasmEx.stack) {
        error.stack = wasmEx.stack;
      }
      console.error(`WebAssembly.Exception in background thread: ${type}: ${message}${wasmEx.stack ? `\n${wasmEx.stack}` : ''}`);
      return error;
    } finally {
      mod.decrementExceptionRefcount(wasmEx);
    }
  }

  if ('addEventListener' in globalThis) { // Do not call for Node.js
    addEventListener('error', ev => {
      if (ev.error && ev.error instanceof WebAssembly.Exception) {
        throw handleWasmExceptionForModule(Module, ev.error);
      }
    });
  }
}

Object.assign(Module, {
  /**
   * Used by `ObservableStream.cpp` to make `cancel()` a no-op after completion.
   * @param {ReadableStream} stream
   * @returns {ReadableStream} `stream`
   */
  withIndirectCancel(stream) {
    const originalCancel = stream.cancel;
    stream.cancel = reason => {
      // Only call if still present
      if (stream.cancel) {
        return originalCancel.call(stream, reason);
      } else {
        console.log('Skipping cancel for completed stream', reason);
      }
    };
    return stream;
  },
});
