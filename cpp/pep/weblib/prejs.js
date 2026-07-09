/**
 * @file Common exception handling logic, and handling of exceptions in background threads
 */

/**
 * - `WebAssembly.Exception`: WASM EH (what we normally use)
 * - `number` (pointer): EMSDK <5.0.5: Emscripten EH without assertions
 * - `Error` (`CppException`): Emscripten EH (EMSDK <5.0.5: only with assertions)
 * @typedef {WebAssembly.Exception | number | Error} WasmException
 */

/**
 * @typedef {Object} MainModule
 * @property {(WasmException) => void} decrementExceptionRefcount
 * @property {(WasmException) => [string, string]} getExceptionMessage
 */

/** @var {MainModule} Module */
/** @var {boolean} ENVIRONMENT_IS_WORKER */

{
  // Also expose these functions for pep.mts
  Object.assign(Module, {
    /**
     * @param {WasmException | *} ex
     * @returns {boolean}
     */
    pepMayBeWasmException(ex) {
      return ex instanceof WebAssembly.Exception
          || (typeof ex === 'number' && ex > 0) //XXX Remove with EMSDK 5.0.5+, also in WasmException typedefs
          || (ex instanceof Error && Object.getPrototypeOf(ex).constructor.name === 'CppException');
    },

    /**
     * @template TReturn
     * @template {WasmException} TException
     * @callback pepConsumeWasmException~callback
     * @param {TException} wasmEx The exception as passed into the function.
     * @returns {TReturn}
     */
    /**
     * Inspects the exception via `callback` and then frees it.
     * @template TReturn
     * @template {WasmException} TException
     * @param {TException} wasmEx
     * @param {pepConsumeWasmException~callback<TReturn, TException>} callback
     * @returns {TReturn}
     */
    pepConsumeWasmException(wasmEx, callback) {
      if (!Module.pepMayBeWasmException(wasmEx)) {
        throw new TypeError('Expected WebAssembly exception, got ' + wasmEx);
      }
      try {
        return callback(wasmEx);
      } finally {
        Module.decrementExceptionRefcount(wasmEx);
      }
    },

    /**
     * Transforms a WASM exception into an Error with the correct message, and stack when available.
     * After obtaining message, frees the WASM exception object.
     * @param {WasmException} wasmEx
     * @returns {Error}
     */
    pepHandleWasmException(wasmEx) {
      return Module.pepConsumeWasmException(wasmEx, () => {
        let [type, message] = Module.getExceptionMessage(wasmEx);
        if (!message || type === message) {
          // Provide more user-friendly error message
          if (type === 'std::bad_alloc') {
            message = 'Out of memory';
          }
        }
        const error = new Error(message || type, {cause: wasmEx});
        if (wasmEx.stack) {
          error.stack = wasmEx.stack;
        }

        let messageAndStack = message;
        if (wasmEx.stack) {
          messageAndStack += `\n${wasmEx.stack}`;
        }
        if (ENVIRONMENT_IS_WORKER) { // Background thread
          // Error: this probably lead to a critical thread (e.g. io_context thread) exiting
          console.error(`WebAssembly Exception in background thread: ${type}: ${messageAndStack}`);
        } else {
          console.warn(`WebAssembly Exception: ${type}: ${messageAndStack}`);
        }
        return error;
      });
    }
  });

  if (ENVIRONMENT_IS_WORKER) {
    // Retrieve exception message on background threads, even in Release mode.
    // See https://github.com/emscripten-core/emscripten/issues/18016
    // Will not call for Node.js, since the method is not there
    globalThis.addEventListener?.('error', ev => {
      if (Module.pepMayBeWasmException(ev.error)) {
        throw Module.pepHandleWasmException(ev.error);
      }
    });
  }
}
