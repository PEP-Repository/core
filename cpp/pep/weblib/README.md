# WebAssembly library: C++ part

This uses the Emscripten toolchain to build a WebAssembly library. See https://emscripten.org/.

To develop, install the Emscripten SDK, source the included `emsdk_env.*` script to add the relevant environment variables, and use `conan_profile_wasm32` from pep/docker-build>.

- The API is exposed using Embind via the `Weblib` class.
- With the `pepWeblib` target, this get compiled to a `.wasm` file plus associcated `.mjs` wrapper including Emscripten runtime. Some (incomplete) TypeScript declarations are also generated.
- This then gets consumed in [`/weblib/pep-repo-client-lib/`](../../../weblib/pep-repo-client-lib/README.md).

For more info, see the [docs](../../../docs/public/development/weblib/developing.md).
