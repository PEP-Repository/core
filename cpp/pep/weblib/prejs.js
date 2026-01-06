/**
 * @file JS helpers used from C++ and background-thread exception handling
 */

/** @var {Object} Module */
/** @var {boolean} ENVIRONMENT_IS_WORKER */

{
  /**
   * @template TReturn
   * @callback consumeWasmException~callback<TReturn>
   * @param {WebAssembly.Exception} wasmEx The exception as passed into the function.
   * @returns {TReturn}
   */
  /**
   * Inspects the exception via `callback` and then frees it.
   * @template TReturn
   * @param {WebAssembly.Exception} wasmEx
   * @param {consumeWasmException~callback<TReturn>} callback
   * @returns {TReturn}
   */
  function consumeWasmException(wasmEx, callback) {
    //TODO Fix when using Emscripten EH, see https://github.com/emscripten-core/emscripten/issues/17115 and pep.mts

    try {
      return callback(wasmEx);
    } finally {
      Module.decrementExceptionRefcount(wasmEx);
    }
  }

  // Retrieve exception message on background threads, even in Release mode.
  // See https://github.com/emscripten-core/emscripten/issues/18016
  if (ENVIRONMENT_IS_WORKER) {
    /**
     * Transforms a WASM exception into an Error with the correct message, and stack when available.
     * After obtaining message, frees the WASM exception object.
     * @param {WebAssembly.Exception} wasmEx
     * @returns {Error}
     */
    function handleBackgroundWasmException(wasmEx) {
      return consumeWasmException(wasmEx, () => {
        const [type, message] = mod.getExceptionMessage(wasmEx);
        const error = new Error(message || type, {cause: wasmEx});
        if (wasmEx.stack) {
          error.stack = wasmEx.stack;
        }
        console.error(`WebAssembly.Exception in background thread: ${type}: ${message}${wasmEx.stack ? `\n${wasmEx.stack}` : ''}`);
        return error;
      });
    }

    if ('addEventListener' in globalThis) { // Do not call for Node.js
      addEventListener('error', ev => {
        if (ev.error && ev.error instanceof WebAssembly.Exception) {
          throw handleBackgroundWasmException(ev.error);
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
    pepWithIndirectCancel(stream) {
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
}
