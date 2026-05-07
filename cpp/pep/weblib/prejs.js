/**
 * @file Background-thread exception handling
 */

/** @var {Object} Module */
/** @var {boolean} ENVIRONMENT_IS_WORKER */

{
  // Code below adapted from pep.mts

  /**
   * @typedef {WebAssembly.Exception | number | Error} WasmException
   */

  /**
   * @param {WasmException | *} ex
   * @returns {boolean}
   */
  function mayBeWasmException(ex) {
    return ex instanceof WebAssembly.Exception
        || (typeof ex === 'number' && ex > 0)
        || (ex instanceof Error && Object.getPrototypeOf(ex).constructor.name === 'CppException');
  }

  /**
   * @template TReturn
   * @template {WasmException} TException
   * @callback consumeWasmException~callback
   * @param {TException} wasmEx The exception as passed into the function.
   * @returns {TReturn}
   */
  /**
   * Inspects the exception via `callback` and then frees it.
   * @template TReturn
   * @template {WasmException} TException
   * @param {TException} wasmEx
   * @param {consumeWasmException~callback<TReturn, TException>} callback
   * @returns {TReturn}
   */
  function consumeWasmException(wasmEx, callback) {
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
     * @param {WasmException} wasmEx
     * @returns {Error}
     */
    function handleBackgroundWasmException(wasmEx) {
      return consumeWasmException(wasmEx, () => {
        let [type, message] = Module.getExceptionMessage(wasmEx);
        if (!message || type === message) {
          if (type === 'std::bad_alloc') {
            message = 'Out of memory';
          }
        }
        const error = new Error(message || type, {cause: wasmEx});
        if (wasmEx.stack) {
          error.stack = wasmEx.stack;
        }
        console.error(`WebAssembly Exception in background thread: ${type}: ${message}${wasmEx.stack ? `\n${wasmEx.stack}` : ''}`);
        return error;
      });
    }

    // Will not call for Node.js, since the method is not there
    globalThis.addEventListener?.('error', ev => {
      if (mayBeWasmException(ev.error)) {
        throw handleBackgroundWasmException(ev.error);
      }
    });
  }
}
