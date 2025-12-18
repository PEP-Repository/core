/**
 * @file XXX Workarounds for missing types
 */

declare namespace WebAssembly {
  /** @see https://developer.mozilla.org/en-US/docs/WebAssembly/Reference/JavaScript_interface/Exception */
  class Exception {
    stack?: string | undefined;
    message?: [string, string] | unknown;
  }
}
